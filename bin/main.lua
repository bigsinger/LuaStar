require'star'

t = {...}
if #t > 0 then
	for k, v in ipairs({...}) do
		print(k..' '..v)
	end
else
	print(star.help())
	os.execute('pause')
end