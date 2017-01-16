size=1000000

name1="vania2.png"
name2="vania.jpg"
name3="words"

name4="temp/vania2.png"
name5="temp/vania.jpg"
name6="temp/words"

echo "===TESTING==="

echo "===Creating file system==="

./a.out -c $size

./a.out -s

echo "===Copy file test==="

./a.out -v $name2

./a.out -s 

./a.out -l

./a.out -i

echo "===Copying additional files==="

./a.out -v $name1

./a.out -v $name3

./a.out -s

./a.out -l

./a.out -i

echo "===Move original files to temp directory==="

mv $name1 temp/
mv $name2 temp/
mv $name3 temp/

echo "===Retrieve files from virtual disk==="

./a.out -m $name1
./a.out -m $name2
./a.out -m $name3

echo "===Comparing md5sum==="

md5sum $name1 $name4
md5sum $name2 $name5
md5sum $name3 $name6




