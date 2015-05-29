class OctoHighlight < Nanoc::Filter
  require 'digest/md5'
  require 'fileutils'
  require 'json'
  require 'pygments'

  identifier :octohl

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
  def strip s
    s.lines.drop_while { |line| line.strip.empty? }.join.rstrip
  end

  def highlight content, options
    HighlightCode::highlight(content, options['language'])
  end

  def panelize(content, options)
    title     = options['title'] ? "<h3 class=\"panel-title path\">#{h options['title']}</h3>" : ''
    download  = options['href'] ? " <a href=\"#{h options['href']}\">download</a>" : ''
    if title.empty? and download.empty?
      caption = ''
    else
      caption = '<figcaption class="panel-heading">%s</figcaption>' % [title + download]
    end

    (<<-EOL
      <figure class="code panel panel-default">
        %s
        <div class="panel-body">%s</div>
      </figure>
     EOL
    ) % [caption, content]
  end

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

  # based on Octopress (http://octopress.org/)
  PYGMENTS_CACHE_DIR = File.expand_path('../../.pygments-cache', __FILE__)
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
