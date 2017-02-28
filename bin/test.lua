require'star'

function main(p1,p2)
	p1 = p1 or 'None'
	p2 = p2 or 'None'
	star.msgbox('input: '..p1..','..p2, 'Hello Lua!')
	return 1
end

return main(...)