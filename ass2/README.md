## prerequisites 
install valgrind, zip and unzip commands
```sh
sudo apt-get install valgrind #ubuntu
yay -S valgrind #arch
```

## development

1. build: `make`

2. run: `./six-degrees`

3. test
```sh
./six-degrees-checker64 ./six-degrees
# check for memory leaks
./six-degrees-checker64 ./six-degrees -m
```
