package libuci_test

import (
	"bufio"
	"fmt"
	"io"
	"net"
	"sync"
	"time"
)

// ServerMode selects how a unified TCPServer serves each accepted
// connection.  One server type covers every network test: true echo,
// "send N bytes then close" (large-payload receive test), and a minimal
// HTTP responder (large HTTP response test).
type ServerMode int

const (
	// ModeEcho echoes every byte received back to the sender (true echo).
	ModeEcho ServerMode = iota
	// ModeSendPayload writes PayloadSize bytes where byte i = i&0xFF,
	// then closes the connection.  Used by the large-payload receive test.
	ModeSendPayload
	// ModeHTTPResponder reads the HTTP request headers up to the blank
	// line, then sends a minimal HTTP/1.0 response whose body is
	// PayloadSize bytes ('a' repeated), then closes.  Used by the
	// large HTTP response test.
	ModeHTTPResponder
)

// TCPServer is a unified TCP server used by the network tests.  It
// listens on a random port and serves every accepted connection
// according to the configured Mode.  Replaces the earlier collection of
// separate echo/payload/http server implementations.
type TCPServer struct {
	listener net.Listener
	mode     ServerMode
	payload  int
	wg       sync.WaitGroup
}

// NewTCPServer starts a unified TCP server on a random port.
// For ModeSendPayload and ModeHTTPResponder, payloadSize sets the
// response length; it is ignored for ModeEcho.
func NewTCPServer(mode ServerMode, payloadSize int) (*TCPServer, error) {
	ln, err := net.Listen("tcp", "0.0.0.0:0")
	if err != nil {
		return nil, fmt.Errorf("starting TCP server: %w", err)
	}
	s := &TCPServer{listener: ln, mode: mode, payload: payloadSize}
	s.wg.Add(1)
	go s.serve()
	return s, nil
}

// Port returns the port the server is listening on.
func (s *TCPServer) Port() int {
	return s.listener.Addr().(*net.TCPAddr).Port
}

func (s *TCPServer) serve() {
	defer s.wg.Done()
	for {
		conn, err := s.listener.Accept()
		if err != nil {
			return // listener closed
		}
		s.wg.Add(1)
		go s.handle(conn)
	}
}

func (s *TCPServer) handle(c net.Conn) {
	defer s.wg.Done()
	defer c.Close()
	switch s.mode {
	case ModeEcho:
		s.echo(c)
	case ModeSendPayload:
		s.writePayload(c)
	case ModeHTTPResponder:
		s.respondHTTP(c)
	}
}

// echo copies every byte received back to the sender using an explicit
// read/write loop (avoids relying on self-splice behaviour of io.Copy).
func (s *TCPServer) echo(c net.Conn) {
	buf := make([]byte, 1024)
	for {
		n, err := c.Read(buf)
		if n > 0 {
			if _, werr := c.Write(buf[:n]); werr != nil {
				return
			}
		}
		if err != nil {
			return
		}
	}
}

// writePayload writes payload bytes where byte i = i&0xFF, then returns
// so the deferred Close fires.
func (s *TCPServer) writePayload(w io.Writer) {
	buf := make([]byte, 256)
	for i := range buf {
		buf[i] = byte(i)
	}
	written := 0
	for written < s.payload {
		n := s.payload - written
		if n > len(buf) {
			n = len(buf)
		}
		nn, err := w.Write(buf[:n])
		written += nn
		if err != nil {
			return
		}
	}
}

// respondHTTP drains the HTTP request headers up to the blank line, then
// sends a minimal HTTP/1.0 response with a PayloadSize-byte body.  A
// read deadline guards against a misbehaving client hanging the server
// goroutine forever.
func (s *TCPServer) respondHTTP(c net.Conn) {
	br := bufio.NewReader(c)
	c.SetReadDeadline(timeNow().Add(10 * time.Second))
	for {
		line, err := br.ReadString('\n')
		if err != nil {
			return
		}
		if line == "\r\n" || line == "\n" {
			break // end of request headers
		}
	}
	c.SetReadDeadline(time.Time{}) // clear deadline for the write phase
	body := make([]byte, s.payload)
	for i := range body {
		body[i] = 'a'
	}
	fmt.Fprintf(c, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", s.payload)
	c.Write(body)
}

// Stop shuts down the server and waits for all in-flight connections.
func (s *TCPServer) Stop() {
	s.listener.Close()
	s.wg.Wait()
}

// UDP echo server

// UDPEchoServer listens on a random port and echoes back all datagrams received.
type UDPEchoServer struct {
	conn *net.UDPConn
	wg   sync.WaitGroup
	done chan struct{}
}

// NewUDPEchoServer starts a UDP echo server on a random port.
func NewUDPEchoServer() (*UDPEchoServer, error) {
	addr, err := net.ResolveUDPAddr("udp", "0.0.0.0:0")
	if err != nil {
		return nil, fmt.Errorf("resolving UDP address: %w", err)
	}
	conn, err := net.ListenUDP("udp", addr)
	if err != nil {
		return nil, fmt.Errorf("starting UDP echo server: %w", err)
	}
	s := &UDPEchoServer{conn: conn, done: make(chan struct{})}
	s.wg.Add(1)
	go s.serve()
	return s, nil
}

// Port returns the port the server is listening on.
func (s *UDPEchoServer) Port() int {
	return s.conn.LocalAddr().(*net.UDPAddr).Port
}

func (s *UDPEchoServer) serve() {
	defer s.wg.Done()
	buf := make([]byte, 4096)
	for {
		select {
		case <-s.done:
			return
		default:
		}
		// Use a short deadline so we can check the done channel
		s.conn.SetReadDeadline(timeNow().Add(200 * time.Millisecond))
		n, remoteAddr, err := s.conn.ReadFromUDP(buf)
		if err != nil {
			continue // timeout or closed
		}
		// Echo back to sender
		s.conn.WriteToUDP(buf[:n], remoteAddr)
	}
}

// Stop shuts down the server.
func (s *UDPEchoServer) Stop() {
	close(s.done)
	s.conn.Close()
	s.wg.Wait()
}

// timeNow is a variable so tests can mock time if needed.
var timeNow = func() time.Time { return time.Now() }

// helpers shared by all servers

// GetLocalIPForDevice returns the local IP address that can reach the Ultimate device.
func GetLocalIPForDevice() (net.IP, error) {
	addr := getDeviceAddress()
	deviceAddr := addr
	if ip := parseIP(addr); ip == nil {
		ips, err := net.LookupHost(addr)
		if err != nil {
			return nil, fmt.Errorf("resolving device address %q: %w", addr, err)
		}
		if len(ips) == 0 {
			return nil, fmt.Errorf("no addresses for %q", addr)
		}
		deviceAddr = ips[0]
	}
	conn, err := net.Dial("udp", deviceAddr+":64")
	if err != nil {
		return nil, fmt.Errorf("dialing device: %w", err)
	}
	defer conn.Close()
	localAddr := conn.LocalAddr().(*net.UDPAddr)
	return localAddr.IP, nil
}

// prepareEchoServerInfo writes server IP and ports into the input buffer.
// Layout: bytes 0-3 = IP, 4-5 = TCP port (big-endian), 6-7 = UDP port (big-endian).
func prepareEchoServerInfo(ip net.IP, tcpPort, udpPort int) []byte {
	ip4 := ip.To4()
	if ip4 == nil {
		panic("expected IPv4 address")
	}
	buf := make([]byte, 8)
	copy(buf[0:4], ip4)
	buf[4] = byte(tcpPort >> 8)
	buf[5] = byte(tcpPort)
	buf[6] = byte(udpPort >> 8)
	buf[7] = byte(udpPort)
	return buf
}
