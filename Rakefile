task :test do
  # see https://stackoverflow.com/questions/13262608/bundle-package-fails-when-run-inside-rake-task
  Bundler.with_clean_env do
    sh 'bundle exec nanoc'
    sh 'bundle exec nanoc check --deploy'
  end
end

task default: :test
