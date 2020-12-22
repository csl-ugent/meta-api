#!/usr/bin/env bash
set -x

cp /home/ubuntu/keepassxc/data/NewDatabase.kdbx /tmp/NewDatabase0.kdbx

#1
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase0.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase1.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli add -u newuser --url https://example.com -g -L 20 NewDatabase.kdbx /newuser-entry > a.log'
#2
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase1.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase2.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli show -s NewDatabase.kdbx /newuser-entry > b.log'
#3
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase2.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase3.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli add -q -u newuser -g -L 20 NewDatabase.kdbx /newentry-quiet > c.log'
#4
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase3.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase4.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli show -s NewDatabase.kdbx /newentry-quiet > d.log'
#5
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase4.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase5.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli add -u newuser2 --url https://example.net -p NewDatabase.kdbx /newuser-entry2 > e.log'
#6
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase5.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase6.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli show -s NewDatabase.kdbx /newuser-entry2 > f.log'
#7
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase6.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase7.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli add -u newuser3 -g -L 34 NewDatabase.kdbx /newuser-entry3 > g.log'
#8
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase7.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase8.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli show -s NewDatabase.kdbx /newuser-entry3 > h.log'
#9
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase8.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase9.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli mkdir NewDatabase.kdbx /new_group > i.log'
#10
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase9.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase10.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli mkdir NewDatabase.kdbx /new_group/newer_group > j.log'
#11
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase10.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase11.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli analyze --hibp /home/ubuntu/keepassxc/data/hibp.txt NewDatabase.kdbx > k.log'
#12
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase11.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase12.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli clip NewDatabase.kdbx "Sample Entry" > l.log || true'
#13
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase12.kdbx NewDatabase.kdbx ; rm -f testCreate1.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase13.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli db-create testCreate1.kdbx > m.log'
#14
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase13.kdbx NewDatabase.kdbx ; rm -f testCreate2.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase14.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli db-create testCreate2.kdbx -k keyfile.txt > n.log'
#15
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase14.kdbx NewDatabase.kdbx ; rm -f testCreate3.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase15.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli db-create testCreate3.kdbx -k keyfile.txt > o.log'
#16
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase15.kdbx NewDatabase.kdbx ; rm -f testCreate4.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase16.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli db-create testCreate4.kdbx -t 500 > p.log'
#17
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase16.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase17.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli rmdir NewDatabase.kdbx "/General" > q.log'
#18
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase17.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase18.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli rmdir NewDatabase.kdbx "/Recycle Bin/General" > r.log'
#19
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase18.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase19.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli rm -q NewDatabase.kdbx "/Sample Entry" > s.log'
#20
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase19.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase20.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli locate NewDatabase.kdbx "Sample Entry" > t.log'
#21
perf stat -o overhead.csv --append -x',' --repeat 10 --pre 'cp /tmp/NewDatabase20.kdbx NewDatabase.kdbx' --post 'cp NewDatabase.kdbx /tmp/NewDatabase21.kdbx' bash -c 'yes a | /home/ubuntu/keepassxc/bin/keepassxc-cli ls NewDatabase.kdbx > u.log'

