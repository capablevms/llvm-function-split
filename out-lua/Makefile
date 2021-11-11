CC ?= clang
SSHPORT ?= 10021
.PHONY: all clean

all: $(patsubst %.bc,lib%.so,$(wildcard *.bc)) lua.shared

clean:
	rm *.so *.bc lua.shared

lua.shared: _main.bc
	$(CC) $(CFLAGS) $< -L. -L/root/lua $(patsubst %.bc,-l%,$(filter-out _main.bc,$(wildcard *.bc))) -L. -L/root/lua -ldl -lm -g -Wl,-rpath,/root/lua -o lua.shared
	
lib%.so: %.bc
	$(CC) $(CFLAGS) -shared -fPIC $< -o lib$(basename $<).so

copy-lua:
	ssh $(SSH_OPTIONS) -p $(SSHPORT) root@localhost "mkdir -p /root/lua"
	scp $(SCP_OPTIONS) -P $(SSHPORT) lua.shared *.so root@localhost:/root/lua/.

copy-exec-tests:
	scp $(SCP_OPTIONS) -P $(SSHPORT) -r ../tests-lua root@localhost:/root/lua/.
	ssh $(SSH_OPTIONS) -p $(SSHPORT) root@localhost "cd /root/lua/tests-lua; sh test.sh"
