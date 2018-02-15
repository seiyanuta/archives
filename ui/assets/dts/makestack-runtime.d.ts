// Generated by dts-bundle v0.7.3

declare module '@makestack/runtime' {
    import { AppAPI } from "@makestack/runtime/dist/api/app";
    import { DeviceAPI } from "@makestack/runtime/dist/api/device";
    import { publish } from "@makestack/runtime/dist/api/event";
    import { eprint, print } from "@makestack/runtime/dist/api/logging";
    import { SerialAPI } from "@makestack/runtime/dist/api/serial";
    import { ConfigAPI } from "@makestack/runtime/dist/api/config";
    import { SubProcessAPI } from "@makestack/runtime/dist/api/subprocess";
    import { TimerAPI } from "@makestack/runtime/dist/api/timer";
    import { GPIOConstructor, I2CConstructor, SPIConstructor } from "@makestack/runtime/dist/types";
    import { logger } from "@makestack/runtime/dist/logger";
    export { print, eprint, publish, logger };
    export const GPIO: GPIOConstructor;
    export const I2C: I2CConstructor;
    export const SPI: SPIConstructor;
    export const Timer: TimerAPI;
    export const Config: ConfigAPI;
    export const App: AppAPI;
    export const Device: DeviceAPI;
    export const SubProcess: SubProcessAPI;
    export const Serial: typeof SerialAPI;
}

declare module '@makestack/runtime/dist/api/app' {
    export class AppAPI {
        enableUpdate(): void;
        disableUpdate(): void;
        onExit(callback: () => void): void;
    }
}

declare module '@makestack/runtime/dist/api/device' {
    export class DeviceAPI {
        getDeviceType(): string;
    }
}

declare module '@makestack/runtime/dist/api/event' {
    export function publish(event: string, data?: string | number): void;
}

declare module '@makestack/runtime/dist/api/logging' {
    export function print(message: string): void;
    export function eprint(message: string): void;
}

declare module '@makestack/runtime/dist/api/serial' {
    export class SerialAPI {
        path: string;
        watching: boolean;
        fd: number;
        baudrate: number;
        constructor(args: {
            path: string;
            baudrate: number;
        });
        static list(): string[];
        configure(baudrate: number): void;
        write(data: Buffer): void;
        read(): Buffer;
        onData(callback: (chunk: Buffer) => void): void;
        onNewLine(callback: (line: string) => void): void;
    }
}

declare module '@makestack/runtime/dist/api/config' {
    export type Configs = {
        [key: string]: string;
    };
    export type onChangeCallback = (value: string) => void;
    export type onCommandCallback = (value: string) => void;
    export class ConfigAPI {
        configs: Configs;
        onChangeCallbacks: {
            [key: string]: onChangeCallback[];
        };
        onCommandCallbacks: {
            [key: string]: onCommandCallback;
        };
        constructor();
        onCommand(key: string, callback: (value: string) => void): void;
        onChange(key: string, callback: (value: string) => void): void;
        update(newConfigs: Configs): Promise<void>;
    }
}

declare module '@makestack/runtime/dist/api/subprocess' {
    export class SubProcessAPI {
        run(argv: string[]): {
            stdout: Buffer;
            stderr: Buffer;
            status: number;
        };
    }
}

declare module '@makestack/runtime/dist/api/timer' {
    export class TimerAPI {
        interval(interval: number, callback: () => void): void;
        loop(callback: () => void): Promise<void>;
        delay(duration: number, callback: () => void): void;
        sleep(duration: number): Promise<void>;
        busywait(usec: number): void;
    }
}

declare module '@makestack/runtime/dist/types' {
    export type GPIOPinMode = 'in' | 'out';
    export type GPIOInterruptMode = 'rising' | 'falling' | 'both';
    export interface GPIOInterface {
        setMode(mode: GPIOPinMode): void;
        write(value: boolean): void;
        read(): boolean;
        onInterrupt(mode: GPIOInterruptMode, callback: () => void): void;
    }
    export interface GPIOConstructor {
        new (args: {
            pin: number;
            mode: GPIOPinMode;
        }): GPIOInterface;
    }
    export type SPIMode = 'MODE0' | 'MDOE1' | 'MODE2';
    export type SPIOrder = 'LSBFIRST' | 'MSBFIRST';
    export interface SPIConstructor {
        new (args: {
            slave?: number;
            mode: SPIMode;
            speed?: number;
            order?: SPIOrder;
            bits?: number;
            ss?: number;
            path?: string;
        }): SPIInterface;
    }
    export interface SPIInterface {
        transfer(tx: number[] | Buffer): Buffer;
    }
    export interface I2CConstructor {
        new (args: {
            address: number;
        }): I2CInterface;
    }
    export interface I2CInterface {
        read(length: number): Buffer;
        write(data: number[] | Buffer): void;
    }
}

declare module '@makestack/runtime/dist/logger' {
    export const logger: {
        debug: (...messages: any[]) => void;
        info: (...messages: any[]) => void;
        error: (...messages: any[]) => void;
        warn: (...messages: any[]) => void;
    };
}

