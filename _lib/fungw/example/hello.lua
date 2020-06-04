-- Public function; atoi() is provided by another objects through fungw
function hello(how_many, who)
	io.write("Hello ", how_many, " ", who, " \n");
	io.write("atoi: ", atoi("42"), "\n");
	return 66;
end

io.write("hello world from init", atoi("32"), "\n");

-- register hello(), to be called from other objects and the host application
fgw_func_reg("hello");
