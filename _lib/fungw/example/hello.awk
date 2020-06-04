# Public function; atoi() is provided by another objects through fungw
function hello(how_many, who) {
	print "Hello", how_many, who
	print "atoi:", atoi("42")
	return 66
}

BEGIN {
	print "hello world from init", atoi("33")

	# register hello(), to be called from other objects and the host application
	fgw_func_reg("hello");
}
