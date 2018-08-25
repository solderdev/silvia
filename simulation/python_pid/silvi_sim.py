import matplotlib.pyplot as plt
import numpy as np
import time
import math
import os.path
import random

class Heater:
    def __init__(self, power, dead_time):
        self.P = power  # W
        self.t_dead = dead_time
        print("init heater with", self.P, "W, dead-time:", self.t_dead, "s")


class Boiler:
    def __init__(self, volume, init_temp, newton_r, temp_env):
        self.V = volume  # liter
        self.init_temp = init_temp
        self.t = init_temp  # C
        self.temp_env = temp_env
        self.r = newton_r  # temperature loss constant
        print("init boiler with", self.V, "l, T =", self.t, ", r =", self.r)
        self.C = 4182 * self.V


class PID:
    def __init__(self, kp, ki, kd, ts, u_min=0, u_max=100, pidtype='A'):
        self.Kp = kp
        self.Ki = ki
        self.Kd = kd
        self.ts = ts
        self.u_min = u_min
        self.u_max = u_max
        self._pidtype = pidtype
        self.history_u = []
        self.history_p = []
        self.history_i = []
        self.history_d = []
        self.history_u = []

        print("init PID type", pidtype, "with P =", kp, ", I =", ki, ", D =", kd, "and Ts =", ts)

        self._u1 = 0
        self._pv1 = 0
        self._pv2 = 0

        self._t1 = -self.ts  # last time updated

    def update(self, t, setpoint, pv):
        # check of we should update or wait
        if t >= self._t1 + self.ts:
            self._t1 = t

            e = setpoint - pv
            part_p = 0.0
            part_i = 0.0
            part_d = 0.0

            # calculate control value
            if self._pidtype is 'A':
                e1 = setpoint - self._pv1
                e2 = setpoint - self._pv2
                # a = self.Kp + self.Ki * self.ts + self.Kd / self.ts
                # if self.Kd > 0:
                #     b = -self.Kp - 2 * self.Kd / self.ts
                # else:
                #     b = 0
                # c = self.Kd / self.ts
                # u = self._u1 + a * e + b * e1 + c * e2
                part_p = self.Kp * (e - e1)
                part_i = self.Ki * self.ts * e
                part_d = (self.Kd * (e - 2 * e1 + e2)) / self.ts
                u = round(self._u1 + part_p + part_i + part_d)
                self.history_p.append(part_p)
                self.history_i.append(part_i)
                self.history_d.append(part_d)
                self.history_u.append(u)
            elif self._pidtype is 'B':
                # u = self._u1 + self._a * e + self._b * self._e1 + self._c * self._e2
                print("type B not implemented")
                exit(-1)
            elif self._pidtype is 'C':
                # part_p = - self.Kp * (pv - self._pv1)
                if abs(e) < 3:
                    part_p = - self.Kp * (pv - self._pv1)
                    part_i = self.Ki * self.ts * e
                else:
                    part_p = - 250 * (pv - self._pv1)
                    part_i = self.ts * e * abs(e)
                # part_i = self.Ki * self.ts * e
                # part_d = - (-self.Kd * (-2*pv + self._pv1 + self._pv2)) / self.ts
                part_d = - (self.Kd * (pv - 2*self._pv1 + self._pv2)) / self.ts
                # u = self._u1 \
                #     - self.Kp * (pv - self._pv1) \
                #     + self.Ki * self.ts * e \
                #     - (self.Kd * (pv - 2*self._pv1 + self._pv2)) / self.ts
                u = round(self._u1 + part_p + part_i + part_d)
                self.history_p.append(part_p)
                self.history_i.append(part_i)
                self.history_d.append(part_d)
                self.history_u.append(u)
            else:
                print("unknown PID type")
                exit(-1)

            # check for saturation
            if u < self.u_min:
                u = self.u_min
            elif u > self.u_max:
                u = self.u_max

            # update state register for next step
            self._pv2 = self._pv1
            self._pv1 = pv
            self._u1 = u
            return u
        else:
            return self._u1


class Simulation:
    def __init__(self, run_time, step_size, boiler, heater, pid):
        if run_time / step_size != round(run_time / step_size):
            print("please make run_time a multiple of step_size!")
            return
        self.t_step = step_size  # s
        self.t_sim = run_time  # s
        print("init simultion with run-time", self.t_sim, "s and step-size", self.t_step, "s")
        self.boiler = boiler
        self.heater = heater
        self.pid = pid
        self.sim_result_t = np.empty(int(run_time / step_size))
        self.sim_result_t[:] = self.boiler.t
        # print("size:", len(self.sim_result_t))
        self.sim_result_on = np.empty(int(run_time / step_size))
        self.sim_result_on[:] = 0
        self.sim_result_heater = np.empty(int(run_time / step_size))
        self.sim_result_heater[:] = 0
        self.sim_result_heater_perc = np.empty(int(run_time / step_size))
        self.sim_result_heater_perc[:] = 0
        self.shot_start = 0
        self.shot_end = 0
        self.target_temp = 100

    def run(self, target_temp=100, shot_start=0, shot_end=0):
        print("running simulation")
        start_time = time.time()
        if 0 < shot_start < shot_end < self.t_sim:
            self.shot_start = shot_start
            self.shot_end = shot_end
        self.target_temp = target_temp
        self.boiler.t = self.boiler.init_temp

        self.pid._pv1 = self.boiler.init_temp
        self.pid._pv2 = self.boiler.init_temp

        backup_p = self.pid.Kp
        backup_i = self.pid.Ki
        backup_d = self.pid.Kd

        step_response_stop = False

        idx = 0
        for t in np.arange(0, self.t_sim, self.t_step):
            # apply old heater value
            self.sim_result_heater[idx] = self.sim_result_heater[idx-1]

            # simulate water dispense for x seconds
            if 0 < self.shot_start < self.shot_end:
                if self.shot_start < t <= self.shot_end:
                    # self.pid.Kp = 45   # 45
                    # self.pid.Ki = 1    # 1
                    # self.pid.Kd = 35   # 35
                    self.boiler.t -= 0.03
                # elif t > self.shot_end and self.boiler.t > self.target_temp +1:
                #     self.pid.Kp = 6
                #     self.pid.Ki = 1
                #     self.pid.Kd = 8
                elif t > self.shot_end:
                    self.pid.Kp = backup_p
                    self.pid.Ki = backup_i
                    self.pid.Kd = backup_d

                # cleaning flush after shot
                # if self.shot_end + 15 < t < self.shot_end + 22:
                #     self.boiler.t -= 0.03
                if self.shot_end + 15 < t < self.shot_end + 25 and self.boiler.t > self.target_temp:
                    self.boiler.t -= 0.03

            # mode = 'step'
            # mode = 'thermostat'
            mode = 'pid'

            if mode is 'step':
                # enable heater in range X
                if 0 < self.boiler.t < 100 and step_response_stop == False:
                    self.sim_result_on[idx] = 1
                    heater_percent = 100
                else:
                    step_response_stop = True
                    heater_percent = 0

            elif mode is 'thermostat':
                # thermostat simulation
                if self.boiler.t > 105:
                    self.sim_result_on[idx] = 0
                    heater_percent = 0
                elif self.boiler.t < 90:
                    self.sim_result_on[idx] = 1
                    heater_percent = 100
                else:
                    self.sim_result_on[idx] = self.sim_result_on[idx-1]

            elif mode is 'pid':
                # PID calculation
                heater_percent = self.pid.update(t, self.target_temp, random.uniform(self.boiler.t - 0.1, self.boiler.t + 0.1))

                if 0:  # manual mode during shot
                    # forceful boiler heating in pre-infusion phase
                    if self.shot_start - 3 < t <= self.shot_start + 0:
                        heater_percent = 100
                        self.pid._u1 = 100

                    # constant heating during shot
                    if self.shot_start < t <= self.shot_end:
                        heater_percent = 90
                        self.pid._u1 = 90

                    # forceful boiler off after shot
                    if self.shot_end < t and self.boiler.t > self.target_temp + 0.5:
                        heater_percent = 0
                        self.pid._u1 = 0

                # PWM modulation
                pwm_steps_num_total = self.pid.ts / self.t_step
                if heater_percent == 0:
                    pwm_steps_on = 0
                else:
                    pwm_steps_on = math.floor(pwm_steps_num_total * heater_percent/100)

                pwm_steps_passed = (t - math.floor(t))
                pwm_steps_passed /= self.t_step
                if pwm_steps_passed >= pwm_steps_on:
                    self.sim_result_on[idx] = 0
                else:
                    self.sim_result_on[idx] = 1
            else:
                print("bad mode")
                exit(-1)

            self.sim_result_heater_perc[idx] = heater_percent

            # calculate heater percent
            if self.sim_result_on[idx]:
                self.sim_result_heater[idx] += self.t_step / self.heater.t_dead
                if self.sim_result_heater[idx] > 1.0:
                    self.sim_result_heater[idx] = 1.0
            else:
                # dirty hack to simulate a soft heater overshoot
                self.sim_result_heater[idx] -= self.sim_result_heater[idx] * self.t_step / self.heater.t_dead * 2.3 # / 2
                if self.sim_result_heater[idx] < 0.0:
                    self.sim_result_heater[idx] = 0.0

            Q = self.heater.P * self.t_step * self.sim_result_heater[idx]
            self.boiler.t += Q / self.boiler.C
            # hack No.1: make term quadratic
            self.boiler.t += (-1 * self.boiler.r * (self.boiler.t**4 - self.boiler.temp_env**4) * self.boiler.t) * self.t_step
            # hack No.2: multiply normal term with boiler temp: almost quadratic (little less effect than **2)
            # self.boiler.t += (-1 * self.boiler.r * (self.boiler.t - self.boiler.temp_env) * self.boiler.t) * self.t_step

            self.sim_result_t[idx] = self.boiler.t
            idx += 1

        print('run time: {} s'.format(time.time() - start_time))

    def print(self):
        if len(self.sim_result_t) == 0:
            print("no data yet")
            return

        print("printing data")

        fig, (ax1a, ax1b) = plt.subplots(2, 1)
        ax1b.set_xlabel('time [s]')
        ax1a.set_ylabel('temp [C]')
        ax2 = ax1a.twinx()
        ax2.set_ylabel('heater percent')

        # ax1.yaxis.set_ticks(np.arange(0, 145 + 1, 10))
        # ax1.set(ylim=[0, 145])
        ax2.yaxis.set_ticks(np.arange(0, 100 + 1, 10))
        ax2.set(ylim=[-10, 110])
        ax1b.set(ylim=[-50, 50])

        x_time = np.arange(0, self.t_sim, self.t_step)

        # # y_heater_on = np.multiply(np.max(self.sim_result_t) - np.min(self.sim_result_t), self.sim_result_on)
        # # y_heater_on += np.min(self.sim_result_t)
        # ax2.plot(x_time, self.sim_result_on * 100, 'b', label='Heater ON')

        # y_heater = np.multiply(np.max(self.sim_result_t) - np.min(self.sim_result_t), self.sim_result_heater)
        # y_heater += np.min(self.sim_result_t)
        if 1:
            ax2.plot(x_time, self.sim_result_heater * 100, 'r', label='Heater effective')
            ax2.plot(x_time, self.sim_result_heater_perc, 'b', label='Heater Percent')

        # plt.axvline(100, color='g')

        if 0 < self.shot_start < self.shot_end:
            plt.axvline(self.shot_start, color='g')
            plt.axvline(self.shot_end, color='g')

        ax1a.plot(x_time, self.sim_result_t, 'k', label='Boiler temperature')

        # plot recording of real sensor
        if 0:
            time = []
            side = []
            top = []
            with open("cold_boot_100perc_heater", "r") as f:
                for line in f:
                    myline = f.readline()
                    if myline != "" and myline[0].isdigit():
                        time_, target_, side_, top_, brewhead_, heater_, pump_, u_, p_, i_, d_ = myline.split(' , ')
                        time.append(int(time_))
                        side.append(float(side_))
                        top.append(float(top_))
                print("REAL DATA: initial temp:", (top[0] + side[0]) / 2)

            time_step = (time[1] - time[0]) / 1000.0
            x_time_s = np.arange(0, len(time) * time_step, time_step)
            # ax1a.plot(x_time_s, temp_side, 'g', label='real temp')

            ax1a.plot(x_time_s, np.add(top, side) / 2, 'pink', label='real temp avg')

        x_time_s = np.arange(0, len(self.pid.history_p), 1)
        ax1b.plot(x_time_s, self.pid.history_p, label='P')
        ax1b.plot(x_time_s, self.pid.history_i, label='I')
        ax1b.plot(x_time_s, self.pid.history_d, label='D')

        combined = np.add(self.pid.history_p, self.pid.history_i)
        combined = np.add(combined, self.pid.history_d)
        ax1b.plot(x_time_s, combined, label='PID')

        ax2.plot(x_time_s, self.pid.history_u, label='u')

        ax1b.legend()
        # ax2.legend()
        plt.legend()
        ax1a.grid(True)
        ax1b.grid(True)
        fig.tight_layout()
        plt.show()


my_heater = Heater(power=750, dead_time=50)  # 1150  dead: 34
# my_boiler = Boiler(volume=0.3, init_temp=90, newton_r=0.0000025, temp_env=23)  # **2 loss term
# my_boiler = Boiler(volume=0.3, init_temp=78.2, newton_r=0.00001, temp_env=23)  # very nice!
# my_boiler = Boiler(volume=0.3, init_temp=77, newton_r=0.00000007, temp_env=30)
# my_boiler = Boiler(volume=0.3, init_temp=26.95, newton_r=0.000000000004, temp_env=27)
my_boiler = Boiler(volume=0.3, init_temp=80, newton_r=0.000000000004, temp_env=27)
# my_pid = PID(kp=25, ki=1.3, kd=5, ts=1, u_min=0, u_max=100)
# my_pid = PID(kp=25, ki=1, kd=10, ts=1, u_min=0, u_max=100, pidtype='C')
# my_pid = PID(kp=30, ki=9, kd=50, ts=1, u_min=0, u_max=100, pidtype='C')

RUN_TIME = 600

if 1:
    # my_pid = PID(kp=25, ki=0.7, kd=20, ts=1, u_min=0, u_max=100, pidtype='C')  # very nice
    # my_pid = PID(kp=25, ki=0.7, kd=0, ts=1, u_min=0, u_max=100, pidtype='C')  # no D
    my_pid = PID(kp=30, ki=0.7, kd=0, ts=1, u_min=0, u_max=100, pidtype='C')  # very nice
    # my_pid = PID(kp=20, ki=1.2, kd=5, ts=1, u_min=0, u_max=100, pidtype='C')  # with new D part
    # my_pid = PID(kp=80, ki=8, kd=90, ts=1, u_min=0, u_max=100, pidtype='C')  # stable long-term
    # my_pid = PID(kp=20, ki=24, kd=22, ts=1, u_min=0, u_max=100, pidtype='C')  # fast for shot

    sim = Simulation(run_time=RUN_TIME, step_size=0.1, boiler=my_boiler, heater=my_heater, pid=my_pid)
    sim.run(target_temp=100, shot_start=460, shot_end=490)

    first_greater_target = np.argmax(sim.sim_result_t > sim.target_temp)
    print("First greater target: ", first_greater_target, " / ", first_greater_target/10, "s")
    variation = sum((sim.sim_result_t[:] - sim.target_temp)**2)

    print("Max: ", np.max(sim.sim_result_t))
    print("Min after SP: ", np.min(sim.sim_result_t[first_greater_target:]))
    # print("Stddev: ", np.std(sim.sim_result_t[first_greater_target:]))
    print("Variation: ", variation)
    sim.print()
    exit(0)

best_p = 0
best_i = 0
best_d = 0
best_diff = 0


def generate_resfile_name(vP, vI, vD):
    string = "./simdata/p{}_i{}_d{}.npy".format(vP, vI, vD)
    return string


for p in np.arange(1.0, 20, 1.0):
    print(p)
    for i in np.arange(0.1, 2, 0.1):
        for d in np.arange(0.0, 5, 0.5):
            results_file = generate_resfile_name(p, i, d)
            sim_results = np.empty(RUN_TIME*10)
            if os.path.isfile(results_file) and os.access(results_file, os.R_OK):
                # print("Loading pre-calculated file: ", results_file)
                sim_results = np.load(results_file)
            else:
                # print("Generate new data: ", results_file)
                my_pid = PID(kp=p, ki=i, kd=d, ts=1, u_min=0, u_max=100, pidtype='C')
                sim = Simulation(run_time=RUN_TIME, step_size=0.1, boiler=my_boiler, heater=my_heater, pid=my_pid)
                sim.run(target_temp=100, shot_start=460, shot_end=490)
                np.save(generate_resfile_name(p, i, d), sim.sim_result_t)
                sim_results = sim.sim_result_t

            # if sim_results[1500] < 100:
            #     # print("Temp not reached in time!", sim_results[1100])
            #     continue

            first_greater_target = np.argmax(sim_results > 100)
            variation = sum((sim_results[:4500] - 100)**2)
            compare_value = variation

            # variation = sum((sim_results[3450:4200] - 100) ** 2)
            # compare_value = variation

            if compare_value < best_diff or best_diff == 0:
                best_p = p
                best_i = i
                best_d = d
                best_diff = compare_value

print("Best P: ", best_p)
print("Best I: ", best_i)
print("Best D: ", best_d)
print("Best Diff: ", best_diff)
