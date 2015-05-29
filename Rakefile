task :test do
  sh('bundle exec nanoc')
  sh('bundle exec nanoc check --deploy')
end

task :default => :test
