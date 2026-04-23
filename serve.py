"""
Simple HTTP server to serve dashboard.html + state.json
Run this BEFORE launching main.exe
"""
import http.server, socketserver, os

PORT = 8080
os.chdir(os.path.dirname(os.path.abspath(__file__)))

class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, format, *args): pass  # suppress noisy logs
    def end_headers(self):
        # Allow fetch() from same origin; no CORS issues
        self.send_header('Cache-Control', 'no-cache, no-store')
        self.send_header('Access-Control-Allow-Origin', '*')
        super().end_headers()

print(f"  Dashboard: http://localhost:{PORT}/dashboard.html")
print(f"  Serving from: {os.getcwd()}")
print(f"  Now run main.exe in another terminal to connect.\n")

with socketserver.TCPServer(("", PORT), Handler) as httpd:
    httpd.serve_forever()
