source 'https://rubygems.org'

gem 'adsf'        # for viewing
gem 'builder'     # for feeds
gem 'pygments.rb' # for syntax highlighting
gem 'compass'     # for advanced CSS
gem 'guard'       # for automatic recompilation
gem 'guard-nanoc' # for automatic recompilation
gem 'haml'        # for layouts with clean sources
gem 'kramdown'    # for advanced markdown
gem 'nanoc'
gem 'nokogiri'    # for parsing HTML
gem 'rake'
gem 'sass', '~> 3.2.13'
gem 'systemu'     # for invoking rsync etc

# for guard
require 'rbconfig'
if RbConfig::CONFIG['target_os'] =~ /(?i-mx:bsd|dragonfly)/
    gem 'rb-kqueue', '>= 0.2'
    gem 'rb-readline' # see https://github.com/guard/guard/wiki/Add-Readline-support-to-Ruby-on-Mac-OS-X
end
