const fs = require("fs");

export class LinuxGPIOAPI {
    public pin: number;

    constructor(args: { pin: number, mode: GPIOPinMode }) {
        if (typeof args.pin !== "number") {
            throw new Error("`this.pin' must be a number");
        }

        this.pin = args.pin;
        this.setMode(args.mode);
    }

    static get OUTPUT() {
        return "out";
    }

    static get INPUT() {
        return "in";
    }

    public setMode(mode: GPIOPinMode) {
        if (mode !== LinuxGPIOAPI.INPUT && mode !== LinuxGPIOAPI.OUTPUT) {
            throw new Error(`invalid pin mode \`${mode}'`);
        }

        if (fs.existsSync(`/sys/class/gpio/gpio${this.pin}`)) {
            fs.writeFileSync(`/sys/class/gpio/unexport`, `${this.pin}`);
        }

        fs.writeFileSync(`/sys/class/gpio/export`, `${this.pin}`);
        fs.writeFileSync(`/sys/class/gpio/gpio${this.pin}/direction`,
        (mode === LinuxGPIOAPI.INPUT) ? "in" : "out");
    }

    public write(value: boolean) {
        fs.writeFileSync(`/sys/class/gpio/gpio${this.pin}/value`, value ? "1" : "0");
    }

    public read() {
        return fs.readFileSync(`/sys/class/gpio/gpio${this.pin}/value`, "utf-8") === "1\n";
    }

    public onInterrupt(mode: GPIOInterruptMode, callback: () => void) {
        if (typeof mode === "function") {
            callback = mode;
            mode = "rising";
        }

        this.setMode(GPIO.INPUT);
        fs.writeFileSync(`/sys/class/gpio/gpio${this.pin}/edge`, mode);
        fs.watch(`/sys/class/gpio/gpio${this.pin}/value`, () => {
            callback();
        });
    }

    public onChange(callback: () => void) {
        this.setMode(GPIO.INPUT);
        fs.writeFileSync(`/sys/class/gpio/gpio${this.pin}/edge`, "both");
        fs.watch(`/sys/class/gpio/gpio${this.pin}/value`, () => {
            callback();
        });
    }
}
