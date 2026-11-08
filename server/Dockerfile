FROM golang:1.22-alpine AS build
WORKDIR /src
COPY go.mod main.go ./
RUN CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o /gta2relay .

FROM scratch
COPY --from=build /gta2relay /gta2relay
EXPOSE 28900/tcp
USER 65534:65534
ENTRYPOINT ["/gta2relay"]
