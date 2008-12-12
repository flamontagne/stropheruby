%w[rubygems rake rake/clean fileutils newgem rubigen].each { |f| require f }
#require File.dirname(__FILE__) + '/lib/strophe_ruby'

# Generate all the Rake tasks
# Run 'rake -T' to see list of generated tasks (from gem root directory)
$hoe = Hoe.new('strophe_ruby', '0.0.4') do |p|
  p.developer('François Lamontagne', 'flamontagne@gmail.com')
  p.changes              = p.paragraphs_of("History.txt", 0..1).join("\n\n")
  p.rubyforge_name       = p.name # TODO this is default value
 # p.extra_dev_deps = [
 #   ['newgem', ">= #{::Newgem::VERSION}"]
 # ]
  
  p.clean_globs |= %w[**/.DS_Store tmp *.log]
  path = (p.rubyforge_name == p.name) ? p.rubyforge_name : "\#{p.rubyforge_name}/\#{p.name}"
  p.remote_rdoc_dir = File.join(path.gsub(/^#{p.rubyforge_name}\/?/,''), 'rdoc')
  p.rsync_args = '-av --delete --ignore-errors'
end

require 'newgem/tasks' # load /tasks/*.rake
Dir['tasks/**/*.rake'].each { |t| load t }

task :default => :compile
