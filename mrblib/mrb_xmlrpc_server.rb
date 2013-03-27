module XMLRPC
  ERR_METHOD_MISSING        = 1
  ERR_UNCAUGHT_EXCEPTION    = 2
  ERR_MC_WRONG_PARAM        = 3
  ERR_MC_MISSING_PARAMS     = 4
  ERR_MC_MISSING_METHNAME   = 5
  ERR_MC_RECURSIVE_CALL     = 6
  ERR_MC_WRONG_PARAM_PARAMS = 7
  ERR_MC_EXPECTED_STRUCT    = 8
 
  class FaultException < StandardError 
    attr_reader :faultCode,:faultString
    def initialize(faultCode, faultString)
      @faultCode = faultCode
      @faultString = faultString
      super(@faultString)
    end
    
    def to_h
      {"faultCode" => @faultCode, "faultString" => @faultString }
    end

    def to_s
      "<?xml version=\"1.0\"?>\r\n<methodResponse>\r\n<fault><value>\r\n<struct>\r\n<member>\r\n<name>faultCode</name>\r\n<value><int>#{@faultCode}</int></value>\r\n</member>\r\n<member>\r\n<name>faultString</name>\r\n<value><string>#{@faultString}</string></value>\r\n</member>\r\n</struct>\r\n</value></fault>\r\n</methodResponse>\r\n"
    end
  end

  class Server

    def call_method(method, params)
      ret = ""
      if self.handlers.has_key?(method)
        ret = self.handlers[method].call(params)
      elsif not self.default_handler.nil?
        ret = self.default_handler.call(params) 
      end
      ret
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

          begin
            method, params = xmlrpc_server.parse_xmlrpc_call(r.body)
            c.read_stop

            ret = xmlrpc_server.call_method(method,params)

            body = xmlrpc_server.serialize_xmlrpc_response(ret)
          rescue ::XMLRPC::FaultException => e
            body = e.to_s
          rescue => e
            body = ::XMLRPC::FaultException.new(ERR_UNCAUGHT_EXCEPTION, "Uncaught exception #{e.message} in method #{method}").to_s
          end

          if !r.headers.has_key?('Connection') || r.headers['Connection'] != 'Keep-Alive'
            c.write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: #{body.size}\r\n\r\n#{body}") do |x|
              c.close() if c
              c = nil
            end
          else
            c.write("HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nContent-Length: #{body.size}\r\n\r\n#{body}")
          end
        }
      }
      UV::run()
    end
  end
end
# vim: set ft=ruby ts=2 sw=2:
