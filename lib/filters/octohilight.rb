# Pygments Nanoc filter producing Twitter Bootstrap panels for code
# highlighting.
#
# Using:
#   The filter use a github-like syntax for codeblock:
#   ```OPTIONS
#   CODE
#   ```
#   where CODE is some code and options is an optional JSON string.
#
# Options:
#   - language: (required) the syntaxe to highligh
#               (ex. ruby, javascript etc.). See the Pygment documentation
#               (http://pygments.org/docs/lexers/) for the full list.
#   - title:    (optional) the panel's title.
#   - linenos:  (true by default) if false the line numbers will not be
#               generated in the table
#   - href:     (optional) an adresse where the code snippet can be downloaded,
#               a download button will be generated.
#
# Example:
#   ```{"lang": "ruby", "title": "The Classic"}
#   puts "Hello World!"
#   ```
# Hacking:
#   If you want to hack this file and move it around be sure to check
#   PYGMENTS_CACHE_DIR which is hardcoded below !
class OctoHighlight < Nanoc::Filter
  require 'json'

  identifier :octohl

  # implement the filter.
  def run(content, params = {})
    content.gsub /```([^\n]*)\n(.+?)```$/m  do
      optstring = $1
      code      = $2
      options   = optstring.empty? ? {} : JSON.parse(optstring)
      txt = strip(code)
      txt = highlight(txt, options) if options['language']
      txt = tableize(txt, options)
      panelize(txt, options)
    end
  end

  # Removes the first blank lines and any whitespace at the end.
  def strip(s)
    s.lines.drop_while { |line| line.strip.empty? }.join.rstrip
  end

  # highlight the given text using the syntaxe provided in options.
  def highlight(content, options)
    HighlightCode::highlight(content, options['language'])
  end

  # generate a bootstrap panel, handling link (href) option and title.
  def panelize(content, options)
    title = download = ''
    if options['href']
      download = <<-EOL
        <a href="#{h options['href']}" title="download" class="btn btn-default btn-xs pull-right">
          <span class="glyphicon glyphicon-download-alt"></span>
        </a>
      EOL
    end
    if options['title']
      title = '<h3 class="panel-title path">%s</h3>' % h(options['title'])
    end
    if title.empty? and download.empty?
      caption = ''
    else
      caption = '<figcaption class="panel-heading clearfix">%s</figcaption>' % [download + title]
    end

    (<<-EOL
      <figure class="code panel panel-default">
        %s
        <div class="panel-body">%s</div>
      </figure>
     EOL
    ) % [caption, content]
  end

  # wrap the highlighted code into a table, handling the linenos option.
  def tableize(content, options)
    lines = ''
    code  = ''
    content.lines.each_with_index do |line, index|
      lines += "<span class=\"line-number\">%d</span>\n" % (index + 1)
      code  += "<span class=\"line\">#{line}</span>"
    end
    lines = '<td class="gutter"><pre class="line-numbers">%s</pre></td>' % lines
    code  = '<td class="code"><pre><code class="%s">%s</code></pre></td>' % [options['language'], code]
    table = '<table><tr>%s</tr></table>' % (options['linenos'] === false ? code : lines + code)
    '<div class="highlight">%s</div>' % table
  end

  # HighlightCode has been copied and hacked from (http://octopress.org/)
  #
  # (The MIT License)
  #
  # Copyright © 2009-2013 Brandon Mathis
  #
  # Permission is hereby granted, free of charge, to any person obtaining a
  # copy of this software and associated documentation files (the ‘Software’),
  # to deal in the Software without restriction, including without limitation
  # the rights to use, copy, modify, merge, publish, distribute, sublicense,
  # and/or sell copies of the Software, and to permit persons to whom the
  # Software is furnished to do so, subject to the following conditions:
  #
  # The above copyright notice and this permission notice shall be included in
  # all copies or substantial portions of the Software.
  #
  # THE SOFTWARE IS PROVIDED ‘AS IS’, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  # IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  # FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  # AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  # LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  # FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  # DEALINGS IN THE SOFTWARE.
  require 'digest/md5'
  require 'fileutils'
  require 'pygments'

  PYGMENTS_CACHE_DIR = File.expand_path('../../../tmp/pygments-cache', __FILE__)
  FileUtils.mkdir_p(PYGMENTS_CACHE_DIR)

  module HighlightCode
    def self.highlight(str, language)
      pygments(str, language).match(/<pre>(.+)<\/pre>/m)[1].to_s.gsub(/ *$/, '') #strip out divs <div class="highlight">
    end

    def self.pygments(code, language)
      if defined?(PYGMENTS_CACHE_DIR)
        path = File.join(PYGMENTS_CACHE_DIR, "#{language}-#{Digest::MD5.hexdigest(code)}.html")
        if File.exist?(path)
          highlighted_code = File.read(path)
        else
          begin
            highlighted_code = Pygments.highlight(code, lexer: language, formatter: 'html', options: {encoding: 'utf-8', startinline: true})
          rescue MentosError
            raise "Pygments can't parse unknown language: #{language}."
          end
          File.open(path, 'w') {|f| f.print(highlighted_code) }
        end
      else
        highlighted_code = Pygments.highlight(code, lexer: language, formatter: 'html', options: {encoding: 'utf-8', startinline: true})
      end
      highlighted_code
    end
  end
end
