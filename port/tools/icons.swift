// Renders SF Symbols to 96x96 alpha masks and prints a C header
import AppKit
let size = 96
let names: [(String, String)] = [
    ("fire", "scope"), ("car", "car.fill"), ("jump", "figure.jumprope"), ("rev", "arrow.uturn.backward"),
    ("wprev", "chevron.left.2"), ("wnext", "chevron.right.2"), ("special", "bolt.fill"), ("menu", "line.3.horizontal"),
]
var out = "// Generated from SF Symbols by port/tools/icons.swift: 96x96 8-bit alpha masks\n#pragma once\nstatic const int kIconSize = 96;\n"
for (id, sym) in names {
    guard let base = NSImage(systemSymbolName: sym, accessibilityDescription: nil) else { fatalError("missing \(sym)") }
    let cfg = NSImage.SymbolConfiguration(pointSize: 64, weight: .semibold)
    let img = base.withSymbolConfiguration(cfg)!
    let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size, pixelsHigh: size, bitsPerSample: 8, samplesPerPixel: 4,
                               hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: size * 4, bitsPerPixel: 32)!
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    let s = img.size
    let scale = min(Double(size) * 0.8 / s.width, Double(size) * 0.8 / s.height)
    let w = s.width * scale, h = s.height * scale
    img.draw(in: NSRect(x: (Double(size) - w) / 2, y: (Double(size) - h) / 2, width: w, height: h))
    NSGraphicsContext.restoreGraphicsState()
    out += "static const unsigned char kIcon_\(id)[\(size * size)] = {"
    var vals: [String] = []
    for y in 0..<size { for x in 0..<size { vals.append(String(rep.colorAt(x: x, y: y).map { Int($0.alphaComponent * 255) } ?? 0)) } }
    out += vals.joined(separator: ",") + "};\n"
}
try! out.write(toFile: CommandLine.arguments[1], atomically: true, encoding: .utf8)
