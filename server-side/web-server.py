#!/usr/bin/env python
"""
Very simple HTTP server in python

Usage:
    ./web-server.py -h
    ./web-server.py -l localhost -p 8000

Send a POST request:
    curl -d "foo=bar&bin=baz" http://localhost:8000
"""
import argparse
import urllib.parse
from http.server import HTTPServer, BaseHTTPRequestHandler

from csv import DictWriter
from pathlib import Path
from datetime import date


class S(BaseHTTPRequestHandler):
    def _set_headers(self):
        self.send_response(200)
        # self.send_header("Content-type", "text/html")
        self.end_headers()

    # def _html(self, message):
    #     """This just generates an HTML document that includes `message`
    #     in the body. Override, or re-write this do do more interesting stuff.
    #     """
    #     content = f"<ht   ml><body><h1>{message}</h1></body></html>"
    #     return content.encode("utf8")  # NOTE: must return a bytes object!

    # def do_GET(self):
    #     self._set_headers()
    #     self.wfile.write(self._html("hi!"))

    # def do_HEAD(self):
    #     self._set_headers()

    def do_POST(self):
        self._set_headers()
        if self.rfile:
            sensors = dict(urllib.parse.parse_qs(self.rfile.read(int(self.headers['Content-Length'])).decode('ascii')))
            _sensors = {key: value[0] for key, value in sensors.items()}

            # write in csv file (weighing=1&feed=2&permeate=3&ph=4&ec=5)
            field_names = ['weighing', 'feed', 'permeate', 'ph', 'ec']
            filename = "csv-files/" + str(date.today()) + ".csv"
            if Path(filename).is_file():
                with open(filename, 'a+', newline='') as write_obj:
                    writer = DictWriter(write_obj, fieldnames=field_names)
                    writer.writerow(_sensors)
            else:
                with open(filename, 'w', newline='') as write_obj:
                    writer = DictWriter(write_obj, fieldnames=field_names)
                    writer.writeheader()
                    writer.writerow(_sensors)


def run(server_class=HTTPServer, handler_class=S, addr="localhost", port=8000):
    server_address = (addr, port)
    httpd = server_class(server_address, handler_class)

    print(f"Starting httpd server on {addr}:{port}")
    httpd.serve_forever()


if __name__ == "__main__":

    parser = argparse.ArgumentParser(description="Run a simple HTTP server")
    parser.add_argument(
        "-l",
        "--listen",
        default="localhost",
        help="Specify the IP address on which the server listens",
    )
    parser.add_argument(
        "-p",
        "--port",
        type=int,
        default=8000,
        help="Specify the port on which the server listens",
    )
    args = parser.parse_args()
    run(addr=args.listen, port=args.port)