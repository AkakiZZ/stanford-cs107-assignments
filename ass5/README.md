პრერეკვიზიტები. valgrind, zip 

```sh
sudo apt-get install valgrind #ubuntu
yay -S valgrind #arch
```

build

```sh
make
```

test

```sh
./bankdriver -t1 -w4  
შექმნის 4 ნაკადს და გაუშვბეს პირველ ტესტზე.  
(7 სხვადასხვა ტესტია (-t1-დან -t7-ის ჩათვლით), გატესტვა შესაძლებელია სხვადასხვა რაოდენობის ნაკადზე).  
./bankdriver -t1 -w4 -b (ამოწმებს ბალანსს).  
./bankdriver -t1 -w2 -y  
(თუ -y-ს გადავცემთ რიცხვს, მაგალითად: -y50, მაშინ 50% ალბათობით შეწყვეტს მიმდინარე ნაკადი მუშაობას).  
valgrind --tool=helgrind ./bankdriver -r -t1 -w4
```

