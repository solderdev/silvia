import matplotlib.pyplot as plt
import numpy as np
import time
import math


class Heater:
    def __init__(self, power, dead_time):
        self.P = power  # W
        self.t_dead = dead_time
        print("init heater with", self.P, "W, dead-time:", self.t_dead, "s")


class Boiler:
    def __init__(self, volume, init_temp, newton_r, temp_env):
        self.V = volume  # liter
        self.t = init_temp  # C
        self.temp_env = temp_env
        self.r = newton_r  # temperature loss constant
        print("init boiler with", self.V, "l, T =", self.t, ", r =", self.r)
        self.C = 4182 * self.V


class PID:
    def __init__(self, kp, ki, kd, ts, u_min=0, u_max=1):
        self.Kp = kp
        self.Ki = ki
        self.Kd = kd
        self.ts = ts
        self.u_min = u_min
        self.u_max = u_max

        self._a = kp + ki * ts + kd / ts
        if kd > 0:
            self._b = -kp - 2 * kd / ts
        else:
            self._b = 0
        self._c = kd / ts

        self._u1 = 0
        self._pv1 = 0
        self._pv2 = 0

        self._t1 = -self.ts  # last time updated

    def update(self, t, setpoint, pv, pidtype='A'):
        # check of we should update or wait
        if t >= self._t1 + self.ts:
            self._t1 = t

            e = setpoint - pv
            # calculate control value
            if pidtype is 'A':
                e1 = setpoint - self._pv1
                e2 = setpoint - self._pv2
                u = self._u1 + self._a * e + self._b * e1 + self._c * e2
            elif pidtype is 'B':
                # u = self._u1 + self._a * e + self._b * self._e1 + self._c * self._e2
                u = 0  # not implemented!
            elif pidtype is 'C':
                u = self._u1 \
                    - self.Kp * (pv - self._pv1) \
                    + self.Ki * self.ts * e \
                    - (self.Kd / self.ts) * (pv - 2*self._pv1 + self._pv2)

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
        print("size:", len(self.sim_result_t))
        self.sim_result_on = np.empty(int(run_time / step_size))
        self.sim_result_on[:] = 0
        self.sim_result_heater = np.empty(int(run_time / step_size))
        self.sim_result_heater[:] = 0

    def run(self, target_temp=100):
        print("running simulation")
        start_time = time.time()

        idx = 0
        for t in np.arange(0, self.t_sim, self.t_step):
            # apply old heater value
            self.sim_result_heater[idx] = self.sim_result_heater[idx-1]

            mode = 'step'
            mode = 'thermostat'
            mode = 'pid'

            if mode is 'step':
                # enable heater in range X
                # if 1 <= t < 8 or 15 <= t < 16 or 18 <= t < 19:
                #     self.sim_result_on[idx] = 1
                if 0 < t < 100: #or 950 < t < 1000:
                    self.sim_result_on[idx] = 1

            elif mode is 'thermostat':
                # thermostat simulation
                if self.boiler.t > 105:
                    self.sim_result_on[idx] = 0
                elif self.boiler.t < 90:
                    self.sim_result_on[idx] = 1
                else:
                    self.sim_result_on[idx] = self.sim_result_on[idx-1]

            elif mode is 'pid':
                # PID calculation
                heater_percent = self.pid.update(t, target_temp, self.boiler.t, 'C') / 100
                # PWM modulation
                pwm_steps_num_total = self.pid.ts / self.t_step
                if heater_percent == 0:
                    pwm_steps_on = 0
                else:
                    pwm_steps_on = math.floor(pwm_steps_num_total * heater_percent)

                pwm_steps_passed = (t - math.floor(t))
                pwm_steps_passed /= self.t_step
                if pwm_steps_passed >= pwm_steps_on:
                    self.sim_result_on[idx] = 0
                else:
                    self.sim_result_on[idx] = 1
            else:
                print("bad mode")
                exit(-1)

            # calculate heater percent
            if self.sim_result_on[idx]:
                self.sim_result_heater[idx] += self.t_step / self.heater.t_dead
                if self.sim_result_heater[idx] > 1.0:
                    self.sim_result_heater[idx] = 1.0
            else:
                # dirty hack to simulate a soft heater overshoot
                self.sim_result_heater[idx] -= self.sim_result_heater[idx] * self.t_step / self.heater.t_dead / 2
                if self.sim_result_heater[idx] < 0.0:
                    self.sim_result_heater[idx] = 0.0

            Q = self.heater.P * self.t_step * self.sim_result_heater[idx]
            self.boiler.t += Q / self.boiler.C
            # hack No.1: make term quadratic
            # self.boiler.t += (-1 * self.boiler.r * (self.boiler.t**2 - self.boiler.temp_env**2)) * self.t_step
            # hack No.2: multiply normal term with boiler temp: almost quadratic (little less effect than **2)
            self.boiler.t += (-1 * self.boiler.r * (self.boiler.t - self.boiler.temp_env) * self.boiler.t) * self.t_step

            self.sim_result_t[idx] = self.boiler.t
            idx += 1

        print('run time: {} s'.format(time.time() - start_time))

    def print(self):
        if len(self.sim_result_t) == 0:
            print("no data yet")
            return

        print("printing data")

        fig, ax1 = plt.subplots()
        ax1.set_xlabel('time [s]')
        ax1.set_ylabel('temp [C]')
        ax2 = ax1.twinx()
        ax2.set_ylabel('heater percent')

        # ax1.yaxis.set_ticks(np.arange(0, 145 + 1, 10))
        # ax1.set(ylim=[0, 145])
        ax2.yaxis.set_ticks(np.arange(0, 100 + 1, 10))

        # plt.axhline(95, color='r')
        # plt.axvline(50, color='r')
        x_time = np.arange(0, self.t_sim, self.t_step)
        ax1.plot(x_time, self.sim_result_t, 'k', label='Boiler temperature')

        # y_heater_on = np.multiply(np.max(self.sim_result_t) - np.min(self.sim_result_t), self.sim_result_on)
        # y_heater_on += np.min(self.sim_result_t)
        # ax2.plot(x_time, self.sim_result_on * 100, 'b', label='Heater ON')

        # y_heater = np.multiply(np.max(self.sim_result_t) - np.min(self.sim_result_t), self.sim_result_heater)
        # y_heater += np.min(self.sim_result_t)
        ax2.plot(x_time, self.sim_result_heater * 100, 'r', label='Heater effective')

        # ax1.legend()
        # ax2.legend()
        plt.legend()
        ax1.grid(True)
        fig.tight_layout()
        plt.show()


my_heater = Heater(power=1000, dead_time=5)  # 1150
# my_boiler = Boiler(volume=0.3, init_temp=90, newton_r=0.0000025, temp_env=23)  # **2 loss term
my_boiler = Boiler(volume=0.3, init_temp=30, newton_r=0.00001, temp_env=23)
# my_pid = PID(kp=25, ki=1.3, kd=5, ts=1, u_min=0, u_max=100)
my_pid = PID(kp=25, ki=1, kd=10, ts=1, u_min=0, u_max=100)
# my_pid = PID(kp=.25, ki=0, kd=0, ts=1, u_min=0, u_max=100)
sim = Simulation(run_time=1000, step_size=0.1, boiler=my_boiler, heater=my_heater, pid=my_pid)

sim.run(target_temp=100)
sim.print()
