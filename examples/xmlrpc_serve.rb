s = XMLRPC::Server.new
s.add_handler("michael.add") { |a,b|
    puts a+b
    a+b
}
s.add_handler("michael.div") { |a,b|
    puts a/b
    a/b
}
s.add_handler("michael.time") { 
    t = Time.now
    puts t
    t
}
s.serve
