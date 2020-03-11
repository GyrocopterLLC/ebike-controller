/******************************************************************************
 * Filename: motor_loop.c
 * Description: The inner loop of the PWM calculation. These functions are
 *              called at a high rate to control the power and speed of
 *              the brushless DC motor.
 ******************************************************************************

 Copyright (c) 2019 David Miller

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */

#include "main.h"
#include "motor_loop.h"
#include "project_parameters.h"

extern Config_Main config_main;

uint8_t lastHallState = 0;
Motor_RunState lastRunState = Motor_Off;

//float HallStateToDriveFloat[8] = HALL_ANGLES_TO_DRIVE_FLOAT;

static void MLoop_Turn_Off_Check(Motor_Controls* cntl) {
    if (cntl->ThrottleCommand <= 0.0f) {
        cntl->state = Motor_Off;
        cntl->speed_cycle_integrator = 0;
        cntl->ThrottleCommand = 0.0f;
        PWM_MotorOFF();
    }
}

void Motor_Loop(Motor_Controls* cntl, Motor_Observations* obv,
        FOC_StateVariables* foc, Motor_PWMDuties* duty) {
    float ipark_a, ipark_b;

    // Regardless of control mode, calculate the Clarke transform.
    // It's outputs are used in power calculations even when the motor isn't
    // being driven.

    // Error reduction - use only the two most lowest duty cycles.
    // Why? The current measurement is done on the low side FET. The more
    // time this FET is turned on, the longer the duration available for
    // measuring current. Also, since this is a balanced three-phase bridge,
    // we can calculate one current if we know the other two:
    // Ia + Ib + Ic = 0
    //      Ia = -(Ib + Ic), Ib = -(Ia + Ic), Ic = -(Ia + Ib)

    float clark_input_a, clark_input_b;
    if((obv->iA > obv->iB) && (obv->iA > obv->iC)) {
        // biggest current is A ==> use B and C
        clark_input_a = -(obv->iB + obv->iC);
        clark_input_b = obv->iB;
    }
    else if((obv->iB) > (obv->iC)) {
        // biggest current is B ==> use A and C
        clark_input_a = obv->iA;
        clark_input_b = -(obv->iA + obv->iC);
    }
    else {
        // biggest current is C ==> use A and B
        clark_input_a = obv->iA;
        clark_input_b = obv->iB;
    }

    dfsl_clarkef(clark_input_a, clark_input_b, &(foc->Clarke_Alpha),
            &(foc->Clarke_Beta));

    // Determine what to do next based on the control state
    switch (cntl->state) {
    case Motor_Off:
        // There's no command to give, so we can skip all that fancy processing.
        duty->tA = 0.0f;
        duty->tB = 0.0f;
        duty->tC = 0.0f;
        dfsl_pid_resetf(foc->Id_PID);
        dfsl_pid_resetf(foc->Iq_PID);
        PWM_MotorOFF();

        break;
    case Motor_Debug:
        // Super simple debugging interface.
        // All three PWMs copy their duty cycle from the throttle position.
        if(lastRunState != Motor_Debug) {
            PHASE_A_PWM();
            PHASE_B_PWM();
            PHASE_C_PWM();
            PWM_MotorON();
        }
        duty->tA = cntl->ThrottleCommand;
        duty->tB = cntl->ThrottleCommand;
        duty->tC = cntl->ThrottleCommand;
        break;
    case Motor_SixStep:
        if (lastRunState != Motor_SixStep) {
            PWM_MotorON();
        }
        MLoop_Turn_Off_Check(cntl);
        // Running the motor in six-step mode.
        // Current monitoring is not used in determining duty cycle, unless
        // a fault condition is met (over-current)
        // State settings:
        //		6 -> +B, -C
        //		2 -> +B, -A
        //		3 -> +C, -A
        //		1 -> +C, -B
        // 		5 -> +A, -B
        //		4 -> +A, -C
        duty->tA = cntl->ThrottleCommand;
        duty->tB = cntl->ThrottleCommand;
        duty->tC = cntl->ThrottleCommand;
        if (lastHallState != obv->HallState) {
            lastHallState = obv->HallState;

            switch (obv->HallState) {
            // Set duty cycles - only one phase is PWM'd. Throttle command is directly
            // sent as power demand (in form of PWM duty cycle)
            // Enable the "participating" phases. One phase pwm, one phase low-side on,
            // and the third phase completely turned off
            case 2:
                PHASE_B_PWM();
                PHASE_A_LOW();
                PHASE_C_OFF();
                break;
            case 6:
                PHASE_C_PWM();
                PHASE_A_LOW();
                PHASE_B_OFF();
                break;
            case 4:
                PHASE_C_PWM();
                PHASE_B_LOW();
                PHASE_A_OFF();
                break;
            case 5:
                PHASE_A_PWM();
                PHASE_B_LOW();
                PHASE_C_OFF();
                break;
            case 1:
                PHASE_A_PWM();
                PHASE_C_LOW();
                PHASE_B_OFF();
                break;
            case 3:
                PHASE_B_PWM();
                PHASE_C_LOW();
                PHASE_A_OFF();
                break;

            default:
                // Oh shit damage control
                cntl->state = Motor_Fault;
                duty->tA = 0.0f;
                duty->tB = 0.0f;
                duty->tC = 0.0f;
                PWM_MotorOFF();
                break;
            }
        }
        break;


        /****
         * This startup routine figures out the zero-level of the current sensors.
         * Forces 50% duty on all phases for a set number of cycles and
         * measures the current. Whatever the value is at 50% duty is the
         * new zero offset.
         *
         * Not run if the motor is spinning. Forcing 50% duty on all phases
         * when the motor is spinning is equivalent to regen braking. Hard.
         * That would be rather unpleasant.
         */
#if 0
    case Motor_Startup:
        if (lastRunState != Motor_Startup) {
            PHASE_A_PWM();
            PHASE_B_PWM();
            PHASE_C_PWM();
            PWM_MotorON();
            StartupCounter = 0;
            iasum = 0;
            ibsum = 0;
            icsum = 0;
        }
        // Check if throttle dropped to zero. We should quit if that happens.
        MLoop_Turn_Off_Check(cntl);
        // Check if motor is running.
        if(HallSensor_Get_Speedf() == 0.0f) {
            // Force outputs all to 50%. This should be zero current.
            duty->tA = 0.5f;
            duty->tB = 0.5f;
            duty->tC = 0.5f;
            if(StartupCounter >= MLOOP_STARTUP_MIN_IGNORE_COUNT + MLOOP_STARTUP_NUM_SAMPLES){
                // Finished - get the average and tell the ADC to use it
                adcSetNull(ADC_IA, (uint16_t)(iasum / MLOOP_STARTUP_NUM_SAMPLES));
                adcSetNull(ADC_IB, (uint16_t)(ibsum / MLOOP_STARTUP_NUM_SAMPLES));
                adcSetNull(ADC_IC, (uint16_t)(icsum / MLOOP_STARTUP_NUM_SAMPLES));
                // Jump to run state
                cntl->state = Motor_AtSpeed;

            } else if(StartupCounter > MLOOP_STARTUP_MIN_IGNORE_COUNT) {
                // Only start summing after some initial dead time
                iasum += adcRaw(ADC_IA);
                ibsum += adcRaw(ADC_IB);
                icsum += adcRaw(ADC_IC);
            }
        } else {
            // Just keep the last zero point, and skip to run state.
            cntl->state = Motor_AtSpeed;
        }

        break;
#endif

// Commenting out this startup routine for now. Now taken care of by the Hall
// sensor keeping track of its own validity instead.
#if 0
    case Motor_Startup:
        // During this startup routine, the FOC is active but the angle
        // of the motor is discontinuous (similar to 6-step)
        // Driven angle is 90deg ahead of the midpoint of the Hall state
        if (lastRunState != Motor_Startup) {
            PHASE_A_PWM();
            PHASE_B_PWM();
            PHASE_C_PWM();
            PWM_MotorON();
            // The PIDs should be reset. If the motor is spinning already,
            // this will cause a huge regenerative braking spike.
            // Zero output on the three phases is like putting out zero
            // volts, and the motor will be acting like a generator
            // with its back-emf. We need to apply voltage at least equal
            // to the back-emf (or a close estimate!) as our true zero-current
            // starting point.
            /*** TODO: Check what the steady-state value of Ui_q tends to be,
             *         see if it's a linear relationship to speed, find the formula.
             *
             *         Seems to be! Looks pretty consistent as a ratio between UI_Q*DCvolts :: speed
             *   TODO: Only do this startup routine when speed is well known (PLL locked)
             *
             *         Well, PLL locks right away even if the speed=0. So that's useless.
             *         */
            foc->Iq_PID->Ui = config_main.kv_volts_per_ehz * obv->RotorSpeed_eHz / cntl->BusVoltage;
            if(foc->Iq_PID->Ui > foc->Iq_PID->OutMax) {
                foc->Iq_PID->Ui = 0.0f; // Don't bother if it's too large. Probably an error state.
            }
            if(foc->Iq_PID->Ui < foc->Iq_PID->OutMin) {
                foc->Iq_PID->Ui = 0.0f; // Likewise if it's less than the minimum.
            }

        }
        MLoop_Turn_Off_Check(cntl);
        if ((obv->HallState > 6) || (obv->HallState < 1)) {
            cntl->state = Motor_Fault;
            duty->tA = 0.0f;
            duty->tB = 0.0f;
            duty->tC = 0.0f;
            PWM_MotorOFF();
        }

        // Clarke transform to 2-phase stationary system
        dfsl_clarkef(obv->iA, obv->iB, &(foc->Clarke_Alpha),
                &(foc->Clarke_Beta));
        dfsl_parkf(foc->Clarke_Alpha, foc->Clarke_Beta,
//                HallStateToDriveFloat[obv->HallState], &(foc->Park_D),
                HallSensor_GetStateMidpoint(obv->HallState), &(foc->Park_D),
                &(foc->Park_Q));
        // Pass filtered current to the PI(D)s
        // Error signals are normalized to 1.0. This allows us to use the same
        // PID gains regardless of current scaling.
        foc->Id_PID->Err = 0.0f
                - ((foc->Park_D) * (config_main.inv_max_phase_current));
        foc->Iq_PID->Err = cntl->ThrottleCommand
                - ((foc->Park_Q) * (config_main.inv_max_phase_current));
        // --- Old version, no normalizing ---
//        foc->Id_PID->Err = 0.0f - foc->Park_D;
//        foc->Iq_PID->Err = (config_main.MaxPhaseCurrent)
//                * (cntl->ThrottleCommand) - foc->Park_Q;
        // --- End old version ---
        //Id_control.Err = 0.0f - Id_Filt.Y;
        //Iq_control.Err = (3.0f)*Throttle_cmd - Iq_Filt.Y;
        // Don't integrate unless the throttle is active
        if (cntl->ThrottleCommand > 0.0f) {
            dfsl_pidf(foc->Id_PID);
            dfsl_pidf(foc->Iq_PID);
        }

        // **************** FORWARD PATH *****************
        // Feed to inverse Park
        dfsl_iparkf(foc->Id_PID->Out, foc->Iq_PID->Out,
//                HallStateToDriveFloat[obv->HallState], &ipark_a, &ipark_b);
                HallSensor_GetStateMidpoint(obv->HallState), &ipark_a,
                &ipark_b);
        // Saturate inputs to unit-length vector
        // Is magnitude of ipark greater than 1?
        if (((ipark_a * ipark_a) + (ipark_b * ipark_b)) > 1.0f) {
            // Trim by scaling by 1.0 / mag(ipark)
            float inv_mag_ipark = 1.0f
                    / sqrtf((ipark_a * ipark_a) + (ipark_b * ipark_b));
            ipark_a = ipark_a * inv_mag_ipark;
            ipark_b = ipark_b * inv_mag_ipark;
        }
        // Inverse Park outputs to space vector modulation, output three-phase waveforms
        dfsl_svmf(ipark_a, ipark_b, &(duty->tA), &(duty->tB), &(duty->tC));

        // Check if we're ready for regular run mode
//        if (HallSensor_Get_Speedf() > MIN_SPEED_TO_FOC) {
        if (HallSensor_Get_Speedf() > config_main.SpeedToFOC) {
            cntl->speed_cycle_integrator = cntl->speed_cycle_integrator + 1;
        } else {
            if (cntl->speed_cycle_integrator > 0) {
                cntl->speed_cycle_integrator = cntl->speed_cycle_integrator - 1;
            }
        }
//        if (cntl->speed_cycle_integrator > SPEED_COUNTS_TO_FOC) {
        if (cntl->speed_cycle_integrator > config_main.CountsToFOC) {
            // Hold off on changing to FOC until the angle is pretty darn close
            // to lining up.
            if (fabsf(
                    HallSensor_GetStateMidpoint(obv->HallState)
                            - obv->RotorAngle) < config_main.SwitchEpsilon)
//          if (fabsf(HallStateToDriveFloat[obv->HallState] - obv->RotorAngle) < FOC_SWITCH_ANGLE_EPS)
                cntl->state = Motor_AtSpeed;
        }
        break;
#endif
    case Motor_AtSpeed:
        if (lastRunState != Motor_AtSpeed) {
            PHASE_A_PWM();
            PHASE_B_PWM();
            PHASE_C_PWM();
            PWM_MotorON();

            // Prevent huge regen current spike when turning on abruptly.
            // Without this feed-forward going into the Iq controller,
            // the output voltage is starting at zero. That's fine for a stopped
            // motor, but it acts as a generator for one that's already spinning.
            // This can cause a big spike and a sudden deceleration while waiting
            // on the integrator to get wind back up to the right value.
            // Using the motor's expected volts per rpm constant to add some
            // voltage when throttle is pulled.
            if(cntl->BusVoltage > 0.01f) // Avoid dividing by zero
            {
                foc->Iq_PID->Ui = config_main.kv_volts_per_ehz * obv->RotorSpeed_eHz / cntl->BusVoltage;
                if(foc->Iq_PID->Ui > foc->Iq_PID->OutMax) {
                    foc->Iq_PID->Ui = 0.0f; // Don't bother if it's too large. Probably an error state.
                }
                if(foc->Iq_PID->Ui < foc->Iq_PID->OutMin) {
                    foc->Iq_PID->Ui = 0.0f; // Likewise if it's less than the minimum.
                }
            }
//	    dfsl_pid_resetf(foc->Id_PID);
//	    dfsl_pid_resetf(foc->Iq_PID);
        }
        MLoop_Turn_Off_Check(cntl);
        // Running full-fledged FOC algorithm now!
        // **************** FEEDBACK PATH *****************
        // Transform sensor readings
        // Clarke transform done above, before the switch statement.
//        dfsl_clarkef(obv->iA, obv->iB, &(foc->Clarke_Alpha),
//                &(foc->Clarke_Beta));
        dfsl_parkf(foc->Clarke_Alpha, foc->Clarke_Beta, obv->RotorAngle,
                &(foc->Park_D), &(foc->Park_Q));
        // Input feedbacks to the Id and Iq controllers
        // Filter the currents
        /*
         Id_Filt.X = foc->Park_D;
         Iq_Filt.X = foc->Park_Q;
         dfsl_biquadf(&Id_Filt);
         dfsl_biquadf(&Iq_Filt);
         */
        // Pass current to the PI(D)s
        // Error signals are normalized to 1.0. This allows us to use the same
        // PID gains regardless of current scaling.
        foc->Id_PID->Err = 0.0f
                - ((foc->Park_D) * (config_main.inv_max_phase_current));
        foc->Iq_PID->Err = cntl->ThrottleCommand
                - ((foc->Park_Q) * (config_main.inv_max_phase_current));
        // --- Old version, no normalizing ---
//        foc->Id_PID->Err = 0.0f - foc->Park_D;
//        foc->Iq_PID->Err = (config_main.MaxPhaseCurrent)
//                * (cntl->ThrottleCommand) - foc->Park_Q;
        // --- End old version ---

        // Don't integrate unless the throttle is active
        if (cntl->ThrottleCommand > 0.0f) {
            dfsl_pidf(foc->Id_PID);
            dfsl_pidf(foc->Iq_PID);
        }

        // **************** FORWARD PATH *****************
        // Feed to inverse Park
        dfsl_iparkf(foc->Id_PID->Out, foc->Iq_PID->Out, obv->RotorAngle,
                &ipark_a, &ipark_b);
        //dfsl_iparkf(0, cntl->ThrottleCommand, obv->RotorAngle, &ipark_a, &ipark_b);
        // Saturate inputs to unit-length vector
        // Is magnitude of ipark greater than 1?
        if (((ipark_a * ipark_a) + (ipark_b * ipark_b)) > 1.0f) {
            // Trim by scaling by 1.0 / mag(ipark)
            float inv_mag_ipark = 1.0f
                    / sqrtf((ipark_a * ipark_a) + (ipark_b * ipark_b));
            ipark_a = ipark_a * inv_mag_ipark;
            ipark_b = ipark_b * inv_mag_ipark;
        }
        // Inverse Park outputs to space vector modulation, output three-phase waveforms
        dfsl_svmf(ipark_a, ipark_b, &(duty->tA), &(duty->tB), &(duty->tC));
        break;

    case Motor_OpenLoop:
        // Current control is active, but only on the D-phase.
        // The forced ramp angle is used instead of the actual motor angle,
        // which means that the motor is locked to a fixed rotational frequency.
        if (lastRunState != Motor_OpenLoop) {
            PHASE_A_PWM();
            PHASE_B_PWM();
            PHASE_C_PWM();
            PWM_MotorON();
            // Resetting the PID means the motor is gonna jump a little bit
            dfsl_pid_resetf(foc->Id_PID);
            dfsl_pid_resetf(foc->Iq_PID);
        }
        MLoop_Turn_Off_Check(cntl);
        // **************** FEEDBACK PATH *****************
        // Transform sensor readings
        // Clarke transform done above, before the switch statement.
//        dfsl_clarkef(obv->iA, obv->iB, &(foc->Clarke_Alpha),
//                &(foc->Clarke_Beta));
        dfsl_parkf(foc->Clarke_Alpha, foc->Clarke_Beta, cntl->RampAngle,
                &(foc->Park_D), &(foc->Park_Q));
        // Input feedbacks to the Id and Iq controllers
        // Pass current to the PI(D)s
        // Error signals are normalized to 1.0. This allows us to use the same
        // PID gains regardless of current scaling.
        foc->Id_PID->Err = cntl->ThrottleCommand
                - ((foc->Park_D) * (config_main.inv_max_phase_current));
        foc->Iq_PID->Err = 0.0f
                - ((foc->Park_Q) * (config_main.inv_max_phase_current));
        // --- Old version, no normalizing ---
//        foc->Id_PID->Err = (config_main.MaxPhaseCurrent)
//                * (cntl->ThrottleCommand) - foc->Park_D;
//        foc->Iq_PID->Err = 0.0f - foc->Park_Q;
        // --- End old version ---

        // Don't integrate unless the throttle is active
        if (cntl->ThrottleCommand > 0.0f) {
            dfsl_pidf(foc->Id_PID);
            dfsl_pidf(foc->Iq_PID);
        }

        // **************** FORWARD PATH *****************
        // Feed to inverse Park
        dfsl_iparkf(foc->Id_PID->Out, foc->Iq_PID->Out, cntl->RampAngle,
                &ipark_a, &ipark_b);
        //dfsl_iparkf(0, cntl->ThrottleCommand, obv->RotorAngle, &ipark_a, &ipark_b);
        // Inverse Park outputs to space vector modulation, output three-phase waveforms
        // Saturate inputs to unit-length vector
        // Is magnitude of ipark greater than 1?
        if (((ipark_a * ipark_a) + (ipark_b * ipark_b)) > 1.0f) {
            // Trim by scaling by 1.0 / mag(ipark)
            float inv_mag_ipark = 1.0f
                    / sqrtf((ipark_a * ipark_a) + (ipark_b * ipark_b));
            ipark_a = ipark_a * inv_mag_ipark;
            ipark_b = ipark_b * inv_mag_ipark;
        }
        dfsl_svmf(ipark_a, ipark_b, &(duty->tA), &(duty->tB), &(duty->tC));
        break;
    case Motor_Fault:
        PWM_MotorOFF();
        duty->tA = 0.0f;
        duty->tB = 0.0f;
        duty->tC = 0.0f;
        break;
    }
    lastRunState = cntl->state;
}
