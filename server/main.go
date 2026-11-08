// GTA2 Native relay server: stands in for DirectPlay's session host. Clients open a TCP
// connection, create or join a game, and every message is forwarded to the other players.
// The game itself runs on the clients in lockstep, so the server knows nothing about it.
//
// Frames are a little-endian u32 length followed by the payload; payload[0] is the opcode.
package main

import (
	"bufio"
	"crypto/rand"
	"encoding/binary"
	"errors"
	"flag"
	"io"
	"log"
	"net"
	"sync"
	"time"
)

// Client to server
const (
	opList = iota + 1
	opCreate
	opJoin
	opClose
	opCreatePlayer
	opDestroyPlayer
	opCreateGroup
	opDestroyGroup
	opAddToGroup
	opDelFromGroup
	opEnum
	opSetGroupData
	opSetFlags
	opSend
	opChat
	opPing
)

// Server to client
const (
	rOK = iota + 100
	rErr
	rList
	rEnum
	evCreatePlayer
	evCreateGroup
	evDestroy
	evAddToGroup
	evDelFromGroup
	evSetGroupData
	evChat
	evMessage
	evSessionLost
)

const (
	maxFrame     = 16 * 1024
	maxClients   = 128
	maxSessions  = 32
	maxPlayers   = 6
	flagJoinOff  = 0x20
	readDeadline = 90 * time.Second
)

type player struct {
	id    uint32
	name  []byte
	data  []byte
	owner *client
}

type group struct {
	id      uint32
	name    []byte
	data    []byte
	members map[uint32]bool
}

type session struct {
	guid    [16]byte
	name    []byte
	flags   uint32
	max     uint32
	host    *client
	players map[uint32]*player
	order   []uint32
	groups  map[uint32]*group
	clients map[*client]bool
}

type client struct {
	conn net.Conn
	out  chan []byte
	sess *session
	once sync.Once
}

// All state is guarded by one lock: traffic is a few small packets per player per frame.
var (
	mu       sync.Mutex
	sessions        = map[[16]byte]*session{}
	nextID   uint32 = 1000
	clients  int
)

type reader struct {
	b   []byte
	err bool
}

func (r *reader) u32() uint32 {
	if len(r.b) < 4 {
		r.err = true
		return 0
	}
	v := binary.LittleEndian.Uint32(r.b)
	r.b = r.b[4:]
	return v
}

func (r *reader) bytes() []byte {
	n := int(r.u32())
	if r.err || n > len(r.b) {
		r.err = true
		return nil
	}
	v := append([]byte(nil), r.b[:n]...)
	r.b = r.b[n:]
	return v
}

func (r *reader) guid() (g [16]byte) {
	if len(r.b) < 16 {
		r.err = true
		return
	}
	copy(g[:], r.b)
	r.b = r.b[16:]
	return
}

type writer struct{ b []byte }

func newMsg(op byte) *writer           { return &writer{b: []byte{op}} }
func (w *writer) u32(v uint32) *writer { w.b = binary.LittleEndian.AppendUint32(w.b, v); return w }
func (w *writer) raw(v []byte) *writer { w.b = append(w.b, v...); return w }
func (w *writer) bytes(v []byte) *writer {
	w.u32(uint32(len(v)))
	w.b = append(w.b, v...)
	return w
}

func (c *client) send(m *writer) {
	select {
	case c.out <- m.b:
	default: // this client stopped reading: drop it rather than stall everyone
		c.shutdown()
	}
}

func (c *client) shutdown() { c.once.Do(func() { c.conn.Close() }) }

func newID() uint32 { nextID++; return nextID }

func (s *session) broadcast(m *writer, except *client) {
	for c := range s.clients {
		if c != except {
			c.send(m)
		}
	}
}

func (c *client) ok(v uint32) { c.send(newMsg(rOK).u32(v)) }
func (c *client) fail()       { c.send(newMsg(rErr)) }

func (c *client) ownsPlayer(id uint32) bool {
	p := c.sess.players[id]
	return p != nil && p.owner == c
}

func removePlayer(s *session, id uint32, except *client) {
	if _, ok := s.players[id]; !ok {
		return
	}
	for _, g := range s.groups {
		if g.members[id] {
			delete(g.members, id)
			s.broadcast(newMsg(evDelFromGroup).u32(g.id).u32(id), nil)
		}
	}
	delete(s.players, id)
	for i, v := range s.order {
		if v == id {
			s.order = append(s.order[:i], s.order[i+1:]...)
			break
		}
	}
	s.broadcast(newMsg(evDestroy).u32(id).u32(1), except)
}

// leave removes a client from its game; when the host goes the game is over for everyone.
func (c *client) leave() {
	s := c.sess
	if s == nil {
		return
	}
	c.sess = nil
	delete(s.clients, c)
	if s.host == c {
		for o := range s.clients {
			o.send(newMsg(evSessionLost))
			o.sess = nil
		}
		delete(sessions, s.guid)
		log.Printf("game %x closed, host left", s.guid[:4])
		return
	}
	for _, id := range append([]uint32(nil), s.order...) {
		if p := s.players[id]; p != nil && p.owner == c {
			removePlayer(s, id, nil)
		}
	}
}

func (c *client) handle(frame []byte) error {
	if len(frame) == 0 {
		return errors.New("empty frame")
	}
	op := frame[0]
	r := &reader{b: frame[1:]}
	mu.Lock()
	defer mu.Unlock()
	s := c.sess
	switch op {
	case opPing:
		c.ok(0)
	case opList:
		m := newMsg(rList)
		var open []*session
		for _, v := range sessions {
			if v.flags&flagJoinOff == 0 && len(v.players) < int(v.max) {
				open = append(open, v)
			}
		}
		m.u32(uint32(len(open)))
		for _, v := range open {
			m.raw(v.guid[:]).bytes(v.name).u32(v.flags).u32(v.max).u32(uint32(len(v.players)))
		}
		c.send(m)
	case opCreate:
		name, flags, max := r.bytes(), r.u32(), r.u32()
		if r.err || s != nil || len(sessions) >= maxSessions {
			c.fail()
			break
		}
		if max == 0 || max > maxPlayers {
			max = maxPlayers
		}
		n := &session{name: name, flags: flags, max: max, host: c, players: map[uint32]*player{},
			groups: map[uint32]*group{}, clients: map[*client]bool{c: true}}
		rand.Read(n.guid[:])
		sessions[n.guid] = n
		c.sess = n
		log.Printf("game %x created (%d open)", n.guid[:4], len(sessions))
		c.send(newMsg(rOK).raw(n.guid[:]))
	case opJoin:
		g := r.guid()
		n := sessions[g]
		if r.err || s != nil || n == nil || n.flags&flagJoinOff != 0 || len(n.players) >= int(n.max) {
			c.fail()
			break
		}
		n.clients[c] = true
		c.sess = n
		c.ok(0)
	case opClose:
		c.leave()
		c.ok(0)
	case opCreatePlayer:
		name, data := r.bytes(), r.bytes()
		if r.err || s == nil || len(s.players) >= int(s.max) {
			c.fail()
			break
		}
		p := &player{id: newID(), name: name, data: data, owner: c}
		s.players[p.id] = p
		s.order = append(s.order, p.id)
		s.broadcast(newMsg(evCreatePlayer).u32(p.id).bytes(p.name).bytes(p.data), c)
		c.ok(p.id)
	case opDestroyPlayer:
		id := r.u32()
		if r.err || s == nil || !c.ownsPlayer(id) {
			c.fail()
			break
		}
		removePlayer(s, id, c)
		c.ok(0)
	case opCreateGroup:
		name, data := r.bytes(), r.bytes()
		if r.err || s == nil || s.host != c {
			c.fail()
			break
		}
		g := &group{id: newID(), name: name, data: data, members: map[uint32]bool{}}
		s.groups[g.id] = g
		s.broadcast(newMsg(evCreateGroup).u32(g.id).bytes(g.name).bytes(g.data), c)
		c.ok(g.id)
	case opDestroyGroup:
		id := r.u32()
		if r.err || s == nil || s.host != c || s.groups[id] == nil {
			c.fail()
			break
		}
		delete(s.groups, id)
		s.broadcast(newMsg(evDestroy).u32(id).u32(0), c)
		c.ok(0)
	case opAddToGroup, opDelFromGroup:
		gid, pid := r.u32(), r.u32()
		if r.err || s == nil || s.groups[gid] == nil || s.players[pid] == nil || (s.host != c && !c.ownsPlayer(pid)) {
			c.fail()
			break
		}
		// Group changes are echoed to the sender too: the game moves its own player on that message
		if op == opAddToGroup {
			s.groups[gid].members[pid] = true
			s.broadcast(newMsg(evAddToGroup).u32(gid).u32(pid), nil)
		} else {
			delete(s.groups[gid].members, pid)
			s.broadcast(newMsg(evDelFromGroup).u32(gid).u32(pid), nil)
		}
		c.ok(0)
	case opEnum:
		if s == nil {
			c.fail()
			break
		}
		m := newMsg(rEnum).u32(uint32(len(s.groups)))
		for _, g := range s.groups {
			m.u32(g.id).bytes(g.name).bytes(g.data).u32(uint32(len(g.members)))
			for _, id := range s.order {
				if g.members[id] {
					m.u32(id)
				}
			}
		}
		m.u32(uint32(len(s.order)))
		for _, id := range s.order {
			p := s.players[id]
			m.u32(p.id).bytes(p.name).bytes(p.data)
		}
		c.send(m)
	case opSetGroupData:
		gid, data := r.u32(), r.bytes()
		if r.err || s == nil || s.host != c || s.groups[gid] == nil {
			c.fail()
			break
		}
		s.groups[gid].data = data
		s.broadcast(newMsg(evSetGroupData).u32(gid).bytes(data), c)
		c.ok(0)
	case opSetFlags:
		f := r.u32()
		if r.err || s == nil || s.host != c {
			c.fail()
			break
		}
		s.flags = f
		c.ok(0)
	case opSend, opChat:
		from, to, payload := r.u32(), r.u32(), r.bytes()
		if r.err || s == nil || !c.ownsPlayer(from) {
			break // no reply: these are fire and forget
		}
		ev := byte(evMessage)
		if op == opChat {
			ev = evChat
		}
		m := newMsg(ev).u32(from).u32(to).bytes(payload)
		if p := s.players[to]; p != nil { // a single player
			p.owner.send(m)
			break
		}
		if g := s.groups[to]; g != nil { // a group
			seen := map[*client]bool{c: true}
			for id := range g.members {
				if o := s.players[id].owner; !seen[o] {
					seen[o] = true
					o.send(m)
				}
			}
			break
		}
		if to == 0 { // everyone
			s.broadcast(m, c)
		}
	default:
		return errors.New("unknown opcode")
	}
	return nil
}

func serve(conn net.Conn) {
	if t, ok := conn.(*net.TCPConn); ok {
		t.SetNoDelay(true)
	}
	mu.Lock()
	if clients >= maxClients {
		mu.Unlock()
		conn.Close()
		return
	}
	clients++
	mu.Unlock()

	c := &client{conn: conn, out: make(chan []byte, 2048)}
	defer func() {
		mu.Lock()
		c.leave()
		clients--
		mu.Unlock()
		c.shutdown()
		close(c.out)
	}()

	go func() {
		w := bufio.NewWriter(conn)
		var head [4]byte
		for f := range c.out {
			binary.LittleEndian.PutUint32(head[:], uint32(len(f)))
			conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
			w.Write(head[:])
			w.Write(f)
			if len(c.out) == 0 && w.Flush() != nil {
				c.shutdown()
			}
		}
	}()

	r := bufio.NewReader(conn)
	var head [4]byte
	for {
		conn.SetReadDeadline(time.Now().Add(readDeadline))
		if _, err := io.ReadFull(r, head[:]); err != nil {
			return
		}
		n := binary.LittleEndian.Uint32(head[:])
		if n == 0 || n > maxFrame {
			return
		}
		frame := make([]byte, n)
		if _, err := io.ReadFull(r, frame); err != nil {
			return
		}
		if err := c.handle(frame); err != nil {
			return
		}
	}
}

func main() {
	addr := flag.String("listen", ":28900", "TCP address to listen on")
	flag.Parse()
	l, err := net.Listen("tcp", *addr)
	if err != nil {
		log.Fatal(err)
	}
	log.Printf("GTA2 relay listening on %s", *addr)
	for {
		conn, err := l.Accept()
		if err != nil {
			log.Print(err)
			time.Sleep(time.Second)
			continue
		}
		go serve(conn)
	}
}
