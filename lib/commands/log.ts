import { Args, CommandBase, Opts } from "../cli";
import { logger } from "../logger";
import { getSdkInstance } from "../platform";

export class Command extends CommandBase {
    public static command = "log";
    public static desc = "";
    public static args = [];
    public static opts = [
        { name: "--platform", desc: "The platform.", required: true, validator: ["firebase"] },
        // TODO:
        { name: "--firebase-project", desc: "The Firebase project name." },
    ];

    public async run(args: Args, opts: Opts) {
        logger.progress(`Reading logs from ${opts.platform}`);
        getSdkInstance(opts.platform).viewLog(opts);
    }
}
