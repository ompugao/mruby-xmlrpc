# mruby-xmlrpc

This gem is alpha version yet. Please send me a pull-request.
This gem depends on mruby-time and libxmlrpc-c.

## Usage

    ### terminal 1

        $ cat xmlrpc_server.rb
        require "xmlrpc/server"
        s = XMLRPC::Server.new(8080)
        s.add_handler("michael.add") do |a,b|
            a + b
        end
        s.set_default_handler do |name, *args|
            raise XMLRPC::FaultException.new(-99, "Method #{name} missing" + " or wrong number of parameters!")
        end
        s.serve

        $ ruby xmlrpc_server.rb

    ### terminal 2

        $ cat xmlrpc_client.rb
        client = XMLRPC::Client.new("localhost","",8080)
        puts client.call("michael.add", 3, 5)
        $ mruby xmlrpc_client.rb
        8
        $ 
