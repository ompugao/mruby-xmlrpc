module XMLRPC
  class FaultException < StandardError ; end
  class Server

    def call_method(method, params)
      begin
        ret = ""
        if self.handlers.has_key?(method)
          ret = self.handlers[method].call(params)
        elsif not self.default_handler.nil?
          ret = self.default_handler.call(params) unless self.default_handler
        end
        [true, ret]
      rescue => e
        [false, e.message]
      end
    end

    def serve
      s = UV::TCP.new
      s.bind(UV::ip4_addr(self.host, self.port))
      xmlrpc_server = self
      s.listen(self.maxConnections) {|x|
        return if x != 0
        c = s.accept
        c.read_start{|b|
          h = HTTP::Parser.new
          r = h.parse_request(b)

          method, params = xmlrpc_server.parse_xmlrpc_call(r.body)
          c.read_stop

          ret = xmlrpc_server.call_method(method,params)

          body = xmlrpc_server.serialize_xmlrpc_response(ret[1])
          if ret[0] 
            if !r.headers.has_key?('Connection') || r.headers['Connection'] != 'Keep-Alive'
              c.write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: #{body.size}\r\n\r\n#{body}") do |x|
                c.close() if c
                c = nil
              end
            else
              c.write("HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nContent-Length: #{body.size}\r\n\r\n#{body}")
            end
          else
            c.write("HTTP/1.1 503 Internal Server Error\r\nContent-Length: #{body.size}\r\n\r\n#{body}")
          end
          c.close
          c = nil
        }
      }
      UV::run()
    end
  end
end
# vim: set ft=ruby ts=2 sw=2:
