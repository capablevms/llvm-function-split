FILE=revcomp.lua-2.lua

run:
	$(LUA) $(FILE) 0 < revcomp-input1000.txt > out

test:
	$(LUA) $(FILE) 0 < revcomp-input1000.txt > out.diff

clean: 
	rm out out.diff
