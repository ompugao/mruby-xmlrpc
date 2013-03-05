MRuby::Gem::Specification.new('mruby-xmlrpc-client') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Shohei Fujii <fujii.shohei@gmail.com>']

  # more options

  # Add compile flags
  spec.cc.flags << `xmlrpc-c-config libwww-client --cflags`.split(' ')

  # Linker settings
  # conf.linker do |linker|
  #   linker.command = ENV['LD'] || 'gcc'
  #   linker.flags = [ENV['LDFLAGS'] || []]
  #   linker.flags_before_libraries = []
  #   linker.libraries = %w()
  #   linker.flags_after_libraries = []
  #   linker.library_paths = []
  #   linker.option_library = '-l%s'
  #   linker.option_library_path = '-L%s'
  #   linker.link_options = "%{flags} -o %{outfile} %{objs} %{libs}"
  # end
 
  # Add cflags to all
  # spec.mruby.cc.flags << '-g'

  require 'pp'
  xmlrpc_libflags = `xmlrpc-c-config libwww-client --libs`.split(' ')
  #spec.linker.library_paths = xmlrpc_libflags.keep_if{|i| i[1]=='L'}.map{|i| i[2,i.size-1]}
  spec.linker.library_paths = xmlrpc_libflags.collect{|i| i if i[1]=='L'}.reject{|i| i==nil}.map{|i| i[2,i.size-1]}
  # Add libraries
  spec.linker.libraries << xmlrpc_libflags.collect{|i| i if i[1]=='l'}.reject{|i| i==nil}.map{|i| i[2,i.size-1]}

  #spec.linker.flags << xmlrpc_libflags.keep_if{|i| (not (i[1]=='l' or i[1]=='L'))}

  # Default building fules
  # spec.rbfiles = Dir.glob("#{dir}/mrblib/*.rb")
  # spec.objs = Dir.glob("#{dir}/src/*.{c,cpp,m,asm,S}").map { |f| objfile(f.relative_path_from(dir).pathmap("#{build_dir}/%X")) }
  # spec.test_rbfiles = Dir.glob("#{dir}/test/*.rb")
  # spec.test_objs = Dir.glob("#{dir}/test/*.{c,cpp,m,asm,S}").map { |f| objfile(f.relative_path_from(dir).pathmap("#{build_dir}/%X")) }
  # spec.test_preload = 'test/assert.rb'

end
