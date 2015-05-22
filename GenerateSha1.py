import hashlib
m=hashlib.sha1()
for i in "00:00:00:00".split(":"):
    m.update(i.decode("hex"))
print m.digest().encode("hex")
