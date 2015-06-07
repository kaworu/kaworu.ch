# All files in the 'lib' directory will be loaded
# before nanoc starts compiling.
require 'json'

def html_title
  if @item[:title]
    "#{@item[:title]} — #{@config[:title]}"
  else
    @config[:title]
  end
end

def canonical_url(item=@item)
  @config[:base_url] + item.path
end

def item_author item
  uri  = (@item[:author_uri]  || @config[:author_uri])
  name = (@item[:author_name] || @config[:author_name])
  uri ? '<a href="%s">%s</a>' % [h(uri), h(name)] : h(name)
end

# shortcut for static stuff
def static_url(path)
  @config[:base_url] + @items['/static' + path + '/'].path
end

def article_path id
  articles.select { |a| File.basename(a.identifier) == id }.first.path
end

def tag_path id
  tags.select { |t| File.basename(t.identifier) == id }.first.path
end

def tags
  @items.select { |i| i[:kind] == 'tag-archive' and i[:tag] }
end

def blog_pages
  @items.select { |i| i[:kind] == 'blog-page' }
end

def youtube(id, ratio16by9=true)
  (<<-EOL
    <div class="embed-responsive embed-responsive-%s">
      <iframe class="embed-responsive-item" src="%s"></iframe>
    </div>
    EOL
  ) % [
    ratio16by9 ? '16by9' : '4by3',
    "//www.youtube.com/v/#{id}"
  ]
end

# borrowed and hacked from Octopress
def include_code file, options
  fn = "static/code/#{file}"
  options['href'] ||= canonical_url @items["/#{fn}/"]
  "```%s\n%s```" % [ options.to_json, File.read(fn) ]
end

def excerptize content
  pos = content.to_s.index(@config[:excerpt_separator])
  if pos
    content.slice(0, pos)
  else
    content
  end
end

def has_excerpt? content
  not content.to_s.index(@config[:excerpt_separator]).nil?
end

class String
  def capitalize0
    length > 1 ? self[0].capitalize + self[1..-1] : capitalize
  end
  def titlecase
    split(/(\W)/).map(&:capitalize0).join
  end
end

# stolen from http://apidock.com/ruby/v1_9_2_180/Time/xmlschema
class Time
  def xmlschema(fraction_digits=0)
    sprintf('%0*d-%02d-%02dT%02d:%02d:%02d',
            year < 0 ? 5 : 4, year, mon, day, hour, min, sec) +
            if fraction_digits == 0
              ''
    else
      '.' + sprintf('%0*d', fraction_digits, (subsec * 10**fraction_digits).floor)
    end +
    if utc?
      'Z'
    else
      off = utc_offset
      sign = off < 0 ? '-' : '+'
      sprintf('%s%02d:%02d', sign, *(off.abs / 60).divmod(60))
    end
  end
end

class Fixnum
    # stolen from the humanize gem :)
    SUB_ONE_HUNDRED = ['zero', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten', 'eleven', 'twelve', 'thirteen', 'fourteen', 'fifteen', 'sixteen', 'seventeen', 'eighteen', 'nineteen', 'twenty', 'twenty-one', 'twenty-two', 'twenty-three', 'twenty-four', 'twenty-five', 'twenty-six', 'twenty-seven', 'twenty-eight', 'twenty-nine', 'thirty', 'thirty-one', 'thirty-two', 'thirty-three', 'thirty-four', 'thirty-five', 'thirty-six', 'thirty-seven', 'thirty-eight', 'thirty-nine', 'forty', 'forty-one', 'forty-two', 'forty-three', 'forty-four', 'forty-five', 'forty-six', 'forty-seven', 'forty-eight', 'forty-nine', 'fifty', 'fifty-one', 'fifty-two', 'fifty-three', 'fifty-four', 'fifty-five', 'fifty-six', 'fifty-seven', 'fifty-eight', 'fifty-nine', 'sixty', 'sixty-one', 'sixty-two', 'sixty-three', 'sixty-four', 'sixty-five', 'sixty-six', 'sixty-seven', 'sixty-eight', 'sixty-nine', 'seventy', 'seventy-one', 'seventy-two', 'seventy-three', 'seventy-four', 'seventy-five', 'seventy-six', 'seventy-seven', 'seventy-eight', 'seventy-nine', 'eighty', 'eighty-one', 'eighty-two', 'eighty-three', 'eighty-four', 'eighty-five', 'eighty-six', 'eighty-seven', 'eighty-eight', 'eighty-nine', 'ninety', 'ninety-one', 'ninety-two', 'ninety-three', 'ninety-four', 'ninety-five', 'ninety-six', 'ninety-seven', 'ninety-eight', 'ninety-nine', 'one hundred']

    def humanize
        if self < SUB_ONE_HUNDRED.size then
            SUB_ONE_HUNDRED[self]
        else
            'more than a hundred'
        end
    end
end
