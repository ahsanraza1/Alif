CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -I include
ALIF    = alif
SMOKE   = tests/smoke
MKEX    = tests/mk_examples
SAMPLE  = tests/add.afbin
ADD     = examples/add.afbin
HELLO   = examples/hello.afbin
AFAS    = afas/afas
ALIFC   = alifc/alifc

.PHONY: all clean smoke examples afas alifc

all: $(ALIF)

$(ALIF): src/alif.c src/load.c src/vm.c include/alif.h include/alf.h include/opcodes.h
	$(CC) $(CFLAGS) -o $(ALIF) src/alif.c src/load.c src/vm.c

$(AFAS): afas/afas.c src/load.c include/alf.h include/opcodes.h
	$(CC) $(CFLAGS) -o $(AFAS) afas/afas.c src/load.c

afas: $(AFAS)

$(ALIFC): alifc/alifc.c lang/keywords.h af8/af8.h
	$(CC) -std=c11 -Wall -Wextra -Werror -o $(ALIFC) alifc/alifc.c

alifc: $(ALIFC)

$(SMOKE): tests/smoke.c src/vm.c include/alif.h include/opcodes.h
	$(CC) $(CFLAGS) -o $(SMOKE) tests/smoke.c src/vm.c

$(MKEX): tests/mk_examples.c src/load.c include/alf.h include/alif.h
	$(CC) $(CFLAGS) -o $(MKEX) tests/mk_examples.c src/load.c

examples: $(MKEX)
	$(MKEX)

$(SAMPLE) $(ADD) $(HELLO): $(MKEX)
	$(MKEX)

smoke: $(SMOKE) $(ALIF) examples
	$(SMOKE)
	./$(ALIF) $(ADD)
	./$(ALIF) $(HELLO)

clean:
	rm -f $(ALIF) $(ALIF).exe $(SMOKE) $(SMOKE).exe $(MKEX) $(MKEX).exe
	rm -f tests/mk_add tests/mk_add.exe $(SAMPLE)
	rm -f $(AFAS) $(AFAS).exe
	rm -f $(ALIFC) $(ALIFC).exe alifc/mk_samples alifc/mk_samples.exe

