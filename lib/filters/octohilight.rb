# Rouge Nanoc filter producing Twitter Bootstrap panels for code
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
#   - lang: (optional)
#     the syntaxe to highligh (eg. ruby). See the Rouge supported languages
#     list at https://github.com/jneen/rouge/wiki/List-of-supported-languages-and-lexers
#   - title: (optional)
#     the panel's title.
#   - linenos: (optional, default: true)
#     if false the line numbers will not be generated in the table
#   - href: (optional)
#     an adresse where the code snippet can be downloaded, a download button
#     will be generated.
#
# Example:
#   ```{"lang": "ruby", "title": "The Classic"}
#   puts "Hello World!"
#   ```

class OctoHighlight < Nanoc::Filter
  require 'json'
  require 'rouge'

  identifier :octohl

  # implement the filter.
  def run(content, params = {})
    content.gsub /```([^\n]*)\n(.+?)```$/m do
      optstring = $1
      code      = $2
      options   = optstring.empty? ? {} : JSON.parse(optstring)
      txt = strip(code)
      txt = highlight(txt, options)
      panelize(txt, options)
    end
  end

  # Removes the first blank lines and any whitespace at the end.
  def strip(s)
    s.lines.drop_while { |line| line.strip.empty? }.join.rstrip
  end

  # highlight the given text using the syntaxe provided in options.
  def highlight(content, options)
    lang = options['lang']
    formatter = Rouge::Formatters::HTML.new
    unless options['linenos'] === false
      formatter = Rouge::Formatters::HTMLTable.new(formatter)
    end
    lexer = Rouge::Lexer.find_fancy(lang, content) || Rouge::Lexers::PlainText
    formatter.format(lexer.lex(content))
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
    content = "<pre>#{content}</pre>" if options['linenos'] === false
    (<<-EOL
      <figure class="code panel panel-default">
        %s
        <div class="panel-body highlight">%s</div>
      </figure>
     EOL
    ) % [caption, content]
  end
end
