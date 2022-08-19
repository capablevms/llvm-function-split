CC ?= clang
USER ?= root
RUNHOST ?= localhost
RUNDIR ?= /root
SSHPORT ?= 10021
.PHONY: all clean

all: $(patsubst %.bc,lib%.so,$(wildcard *.bc)) lua.shared

clean:
	rm *.so *.bc lua.shared

lua.shared: _main.bc
	$(CC) $(CFLAGS) $< -L. $(patsubst %.bc,-l%,$(filter-out _main.bc,$(wildcard *.bc))) -L. -ldl -lm -g -Wl,-rpath,$(RUNDIR)/lua -o lua.shared

lib%.so: %.bc
	$(CC) $(CFLAGS) -shared -fPIC $< -o lib$(basename $<).so

copy-lua:
	ssh $(SSH_OPTIONS) -p $(SSHPORT) $(USER)@$(RUNHOST) "mkdir -p $(RUNDIR)/lua"
	scp $(SCP_OPTIONS) -P $(SSHPORT) lua.shared *.so $(USER)@$(RUNHOST):$(RUNDIR)/lua/.

copy-exec-tests:
	scp $(SCP_OPTIONS) -P $(SSHPORT) -r ../tests-lua $(USER)@$(RUNHOST):$(RUNDIR)/lua/.
	ssh $(SSH_OPTIONS) -p $(SSHPORT) $(USER)@$(RUNHOST) "cd $(RUNDIR)/lua/tests-lua; sh test.sh"