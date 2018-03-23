import os
import datetime

threads = [16, 4, 1]
nums = [("100000000000", 999), ("18446744073709551616", "40")]
for start, count in nums:
    for thread in threads:
        durations = []
        for i in range(0, 3):
            print "Trial %i: %s %s %s" % (i, thread, start, count)
            start_time = datetime.datetime.now()
            os.system("./main %s %s %s > /dev/null 2>&1" % (thread, start, count))
            durations.append((datetime.datetime.now() - start_time).total_seconds())
        print "%s %s %s: %s\n" % (thread, start, count, sorted(durations)[1])
