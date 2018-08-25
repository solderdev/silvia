import numpy as np

# pwm = np.zeros((10, 10))
# percent = 25
#
# # fill array in each 10-percent field (tens) with 1s according to number of every 10%
# max_ones = int(percent/10)
# for tens in range(0, 10):
#     for ones in range(0, max_ones):
#         pwm[tens][ones] = 1
#
# # now handle remaining 0% to 9% on the first digit of the overall percent value
# max_tens = int(percent - int(percent/10)*10)
# for tens in range(0, max_tens):
#     pwm[tens][max_ones] = 1
#
# for idx, val in enumerate(pwm):
#     print(str(idx), val)

pwm = np.zeros(10)
percent = 25

# fill array in each 10-percent field (tens) with 1s according to number of every 10%
max_ones = int(percent/10)
for tens in range(0, 10):
    for ones in range(0, max_ones):
        pwm[tens] += 1

# now handle remaining 0% to 9% on the first digit of the overall percent value
max_tens = int(percent - int(percent/10)*10)
for tens in range(0, max_tens):
    pwm[tens] += 1

for idx, val in enumerate(pwm):
    print(str(idx), val)


for period in range(0, 100):
    if period % 10 == 0:
        print("/// ", int(period/10))
    if pwm[int(period / 10)] > period % 10:
        print(1)
    else:
        print(0)