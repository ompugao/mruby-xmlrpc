#!mruby
a = XMLRPC::Client.new("localhost","",8080)
begin
  puts "michael.add 3,5"
  puts a.call("michael.add",3,5)
  
  puts "michael.div 4,3"
  puts a.call("michael.div",4,3)

  puts "michael.time"
  ret =  a.call("michael.time",Time.now)
  puts ret.class
  puts ret
  puts ret.zone
rescue RuntimeError => e
  puts e
end
# vim: set ft=ruby ts=2 sw=2:
