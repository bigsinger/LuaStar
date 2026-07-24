local star = require "star"

print(("LuaStar %s，运行于 %s"):format(
    star.version(), star.lua_version()))
print("当前脚本：" .. (star.script_path() or "<内存脚本>"))

for index, value in ipairs({...}) do
    print(("参数[%d]：%s"):format(index, value))
end
