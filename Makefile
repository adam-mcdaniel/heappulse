build: hook.so libbkmalloc.so
	./build-allocator.sh

.PHONY: clean
clean:
	rm *.csv
	cd test; rm *.exe
	rm *.so
