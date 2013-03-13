MRuby::Gem::Specification.new('mruby-xmlrpc') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Shohei Fujii <fujii.shohei@gmail.com>']
  #spec.description = "The version of libxmlrpc-c is 1.16.33-3.1ubuntu5.1"
  # more options

  # Add compile flags
  spec.cc.flags << `xmlrpc-c-config libwww-client --cflags`.split(' ')

  # Add cflags to all
  # spec.mruby.cc.flags << '-g'

  xmlrpc_libflags = `xmlrpc-c-config libwww-client --libs`.split(' ')
  #spec.linker.library_paths = xmlrpc_libflags.keep_if{|i| i[1]=='L'}.map{|i| i[2,i.size-1]}
  spec.linker.library_paths = xmlrpc_libflags.collect{|i| i if i[1]=='L'}.reject{|i| i==nil}.map{|i| i[2,i.size-1]}
  # Add libraries
  spec.linker.libraries << xmlrpc_libflags.collect{|i| i if i[1]=='l'}.reject{|i| i==nil}.map{|i| i[2,i.size-1]}
  spec.linker.flags << xmlrpc_libflags.collect{|i| i if (not (i[1]=='l' or i[1]=='L'))}.reject{|i| i==nil}
  #spec.linker.flags = "-Wl,-Bsymbolic-functions -Wl,-z,relro"

  # Default building fules
  # spec.rbfiles = Dir.glob("#{dir}/mrblib/*.rb")
  # spec.objs = Dir.glob("#{dir}/src/*.{c,cpp,m,asm,S}").map { |f| objfile(f.relative_path_from(dir).pathmap("#{build_dir}/%X")) }
  # spec.test_rbfiles = Dir.glob("#{dir}/test/*.rb")
  # spec.test_objs = Dir.glob("#{dir}/test/*.{c,cpp,m,asm,S}").map { |f| objfile(f.relative_path_from(dir).pathmap("#{build_dir}/%X")) }
  # spec.test_preload = 'test/assert.rb'

end
