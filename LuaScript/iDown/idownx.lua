require('common')
local json=require('json')

idown.ERR_STOP              = -1
idown.ERR_SUCCESS           = 0
idown.ERR_NO_FILE           = 200
idown.ERR_SINGLE_FILE       = 201
idown.ERR_MULTI_FILES       = 202
idown.ERR_CONTINUE_PARSE    = 301

idown.CP_UTF8 = 65001

idown.encode = function (t)
	return json:encode(t)
end

idown.decode = function (t)
	return json:decode(t)
end

