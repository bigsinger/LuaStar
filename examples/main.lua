local star = require "star"

local root, file = star.path()
local starVersion, luaVersion = star.version()

print(("Star %s / %s"):format(starVersion, luaVersion))
print("当前脚本：" .. root .. file)

for index, value in ipairs({...}) do
    print(("参数[%d]：%s"):format(index, value))
end
