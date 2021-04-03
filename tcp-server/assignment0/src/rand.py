import string
import random

f = open("my_test.txt", "w")

for i in range(6000):
   for j in range(60):
      f.write(random.choice(string.ascii_letters))


f.close()

