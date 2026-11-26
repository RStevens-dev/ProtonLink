// The Attenuator publishes complete /status objects on /ws.
export function subscribeTelemetry(url, receive, {open = address => new WebSocket(address),
  later = setTimeout, cancel = clearTimeout} = {}) {
  let socket, retry, deadline, stopped = false, delay = 500;
  function reconnect() {
    if (stopped) return;
    retry = later(connect, delay);
    delay = Math.min(15000, delay * 2);
  }
  function connect() {
    try { socket = open(url); } catch { reconnect(); return; }
    const current = socket;
    deadline = later(() => current.close(), 5000);
    current.onopen = () => cancel(deadline);
    current.onmessage = event => {
      if (stopped || socket !== current || typeof event.data !== "string" || event.data.length > 65536) return;
      try {
        if (receive(JSON.parse(event.data))) delay = 500;
      } catch { /* HTTP reconciliation continues for unsupported frames. */ }
    };
    current.onerror = () => current.close();
    current.onclose = () => {
      if (socket !== current) return;
      cancel(deadline);
      socket = null;
      reconnect();
    };
  }
  connect();
  return () => {
    stopped = true;
    cancel(retry);
    cancel(deadline);
    socket?.close();
  };
}
