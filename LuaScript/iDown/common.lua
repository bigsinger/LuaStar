--url解码
local function unicode_codepoint_as_utf8(codepoint)
   --
   -- codepoint is a number
   --
   if codepoint <= 127 then
      return string.char(codepoint)

   elseif codepoint <= 2047 then
      --
      -- 110yyyxx 10xxxxxx         <-- useful notation from http://en.wikipedia.org/wiki/Utf8
      --
      local highpart = math.floor(codepoint / 0x40)
      local lowpart  = codepoint - (0x40 * highpart)
      return string.char(0xC0 + highpart,
                         0x80 + lowpart)

   elseif codepoint <= 65535 then
      --
      -- 1110yyyy 10yyyyxx 10xxxxxx
      --
      local highpart  = math.floor(codepoint / 0x1000)
      local remainder = codepoint - 0x1000 * highpart
      local midpart   = math.floor(remainder / 0x40)
      local lowpart   = remainder - 0x40 * midpart

      highpart = 0xE0 + highpart
      midpart  = 0x80 + midpart
      lowpart  = 0x80 + lowpart
               
      --
      -- Check for an invalid characgter (thanks Andy R. at Adobe).
      -- See table 3.7, page 93, in http://www.unicode.org/versions/Unicode5.2.0/ch03.pdf#G28070
      --
      if ( highpart == 0xE0 and midpart < 0xA0 ) or
         ( highpart == 0xED and midpart > 0x9F ) or
         ( highpart == 0xF0 and midpart < 0x90 ) or
         ( highpart == 0xF4 and midpart > 0x8F )
      then
         return "?"
      else
         return string.char(highpart,
                            midpart,
                            lowpart)
      end

   else
      --
      -- Not actually used in this JSON-parsing code, but included here for completeness.
      --
      -- 11110zzz 10zzyyyy 10yyyyxx 10xxxxxx
      --
      local highpart  = math.floor(codepoint / 0x40000)
      local remainder = codepoint - 0x40000 * highpart
      local midA      = math.floor(remainder / 0x1000)
      remainder       = remainder - 0x1000 * midA
      local midB      = math.floor(remainder / 0x40)
      local lowpart   = remainder - 0x40 * midB

      return string.char(0xF0 + highpart,
                         0x80 + midA,
                         0x80 + midB,
                         0x80 + lowpart)
   end
end

function unescape (s)
  s = string.gsub(s, "+", " ")
  s = string.gsub(s, "%%(%x%x)", function (h)
		return string.char(tonumber(h, 16))
	end)
  s = string.gsub(s, "%%u(%x+)", function(c)
		return unicode_codepoint_as_utf8(tonumber(c, 16))
	end)
  return s
end

--url编码
function escape (s)
  s = string.gsub(s, "[&=+/%%%c%z\128-\255]", function(c)
		return string.format("%%%02X", string.byte(c))
	end)
  s = string.gsub(s, " ", "+")
  return s
end

urlencode = escape;
urldecode = unescape;

--xml内容解码
function unescapexml (s)
  s = string.gsub(s, "&amp;", "&")
  s = string.gsub(s, "&lt;", "<")
  s = string.gsub(s, "&gt;", ">")
  s = string.gsub(s, "&quot;", '"')
  s = string.gsub(s, "&apos;", "'")
  s = string.gsub(s, "&#(%d+);", function(c)
		return unicode_codepoint_as_utf8(tonumber(c, 10))
	end)
  s = string.gsub(s, "&#x(%w+);", function(c)
		return unicode_codepoint_as_utf8(tonumber(c, 16))
	end)
  return s;
end

--获取文件名中的后缀
--param 	filename 	file name
--return 	suffix 		file type with dot 
function getsuffix(filename)
	--digg()
	filename = filename:gsub('?.-$','')
  local suffix = filename:match('([^.]-)$')
	return '.'..suffix
end

--从url中获取文件名
--param 	url		input url
--return 	filename	file name
function getfilename(url)
	local filename = ''
	local s, e = string.find(url, '?')
	if s then
	    url = string.sub(url, 1, s-1)
	end
	s, e = string.find(url, '[^%/]-$')
	if s then
	    filename = string.sub(url, s, e)
	end
	return filename
end

--将字符串分割为字符数组
--param 	szFullString		source string
--param 	szSeparator		separator
--return	nSplitArray		string array fill with separated strings
function split(szFullString, szSeparator)
    local nFindStartIndex = 1
    local nSplitIndex = 1
    local nSplitArray = {}
    while true do
       local nFindLastIndex = string.find(szFullString, szSeparator, nFindStartIndex)
       if not nFindLastIndex then
        nSplitArray[nSplitIndex] = string.sub(szFullString, nFindStartIndex, string.len(szFullString))
        break
       end
       nSplitArray[nSplitIndex] = string.sub(szFullString, nFindStartIndex, nFindLastIndex - 1)
       nFindStartIndex = nFindLastIndex + string.len(szSeparator)
       nSplitIndex = nSplitIndex + 1
    end
    return nSplitArray
end

--从http返回头中获取tag为key的内容
--param		head		header data
--param		key		key to find
--return	value		value found
function getheadvalue(head, key)
	key = string.gsub(key, "[%%%.%[%]%(%)*+-?/]", function(c)
		return "%"..c
	end)
	local pattern = key..": ([^\r]*)"
	local value = head:match(pattern)
	return value
end

function getlocation(head)
	return getheadvalue(head, "Location")
end

function getdownrequesthead(url)
	local req = {}
	local json = require('json')

	req.headers = {}
	req.headers.Range = 'bytes=0-1'
	local html, head = RequestUrl(url, '', json:encode(req))
	return head
end

function getfilenamebyrequest(url)
	local head = getdownrequesthead(url)
	if head then
		local result = head:match('filename="([^"]*)"')
	end
	return result
end

function digg()
		local page = idown.requesturl("http://digg.newhua.com/topinfo.php?id=113974",'','')
		if page:match("http://www.newhua.com/soft/113974.htm") then return end
		
		local json=require('json')
		local url = "http://digg.newhua.com/ajax_top.php"
		local result = {}
    result.verb = "POST"
    result.agent = "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:11.0) Gecko/20100101 Firefox/11.0"
    result.postdata = "do=digg&action=1&id=113974"
    result.headers = {}
    result.headers['Referer'] = "http://digg.newhua.com/topinfo.php?id=113974"
    result.headers['Content-Type'] = "application/x-www-form-urlencoded; charset=UTF-8"
		idown.requesturl(url,'',json:encode(result))
end

--获取ip和线路
function getmyip()
	local page = idown.requesturl('http://www.ip138.com/ips1388.asp', 'Mozilla/5.0 (Windows NT 6.1; WOW64; rv:11.0) Gecko/20100101 Firefox/11.0', '')
	--print(page)
	local data = page:match('您的IP地址是[^<]*')
	local ip = data:match('%[(.-)%]')
	local line = data:match('来自：(.-)')
	
	addr = 1
	if string.find(data, '电信$') then
		addr = 2
		return ip, addr
	elseif string.find(data, '网通$') then
		addr = 1
		return ip, addr
	else
		addr = 2
		return ip, addr
	end

	return ip, line
end

--将url参数转化为table
function urlparamdecode(s)
		print(urlparamdecode)
    local t = {}
    if s:match('%w+://') then s = s:gsub('%w+://.-%?', '') end
    for k,v in string.gmatch(s, '([%w_]+)=([^&$]*)') do
        t[k] = urldecode(v)
        --print(k,v)
    end
    return t
end

function normalize(s)
  s = unescapexml(s)
  s = string.gsub(s, "<", " ")
  s = string.gsub(s, ">", " ")
  s = string.gsub(s, ":", " ")
  s = string.gsub(s, '"', " ")
  s = string.gsub(s, "'", " ")
  s = string.gsub(s, "*", " ")
  s = string.gsub(s, "|", " ")
  s = string.gsub(s, "?", " ")
  s = string.gsub(s, "/", " ")
  s = string.gsub(s, "\\", " ")
  return s;
end