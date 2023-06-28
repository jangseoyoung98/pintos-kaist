cd ..
make clean
source ./activate
cd vm
make clean
make
cd build

pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/bad-read:bad-read --swap-disk=4 -- -q   -f run bad-read < /dev/null 2> tests/userprog/bad-read.errors |tee tests/userprog/bad-read.output
perl -I../.. ../../tests/userprog/bad-read.ck tests/userprog/bad-read tests/userprog/bad-read.result

# pintos -v -k -T 60 -m 20 --fs-disk=10 -p tests/vm/pt-grow-stack:pt-grow-stack --swap-disk=4 -- -q -f run pt-grow-stack
# perl -I../.. ../../tests/vm/pt-grow-stack.ck tests/vm/pt-grow-stack tests/vm/pt-grow-stack.result

# pintos -v -k -T 60 -m 20 --fs-disk=10 -p tests/vm/pt-grow-bad:pt-grow-bad --swap-disk=4 -- -q   -f run pt-grow-bad 
# perl -I../.. ../../tests/vm/pt-grow-bad.ck tests/vm/pt-grow-bad tests/vm/pt-grow-bad.result

# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/vm/pt-big-stk-obj:pt-big-stk-obj --swap-disk=4 -- -q   -f run pt-big-stk-obj
# perl -I../.. ../../tests/vm/pt-big-stk-obj.ck tests/vm/pt-big-stk-obj tests/vm/pt-big-stk-obj.result


