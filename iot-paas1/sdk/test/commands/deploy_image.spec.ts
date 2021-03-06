import { expect } from 'chai';
import * as child_process from 'child_process';
import * as fs from 'fs';
import * as path from 'path';
import * as nock from 'nock';
import * as sinon from 'sinon';
import * as mockfs from 'mock-fs';
import {
    CONFIG_FILES, login, createApp, createApiRequest, createZip
} from '../helpers';

const deployImageCommand = require('../../dist/commands/deploy_image').main

describe('deploy-image command', function() {
    beforeEach(function () {
        mockfs(Object.assign({}, CONFIG_FILES));
        this.appName = 'hello-world';
        this.appDir = createApp(this.appName);
        login();
    })

    afterEach(function () {
        mockfs.restore()
        nock.cleanAll()
    })

    it('invokes POST /apps/:name/deployments', async function () {
        const runtimeDownloadRequest = createApiRequest()
            .get("/plugins/_/_/runtime")
            .reply(200, await createZip({ 'start.js': '' }))

        const deployRequest = createApiRequest()
            .post(`/apps/${this.appName}/deployments`)
            .reply(200, {})

        const imageFilepath = '/user/app.zip'
        fs.writeFileSync(imageFilepath, await createZip({}))

        await deployImageCommand({ image: imageFilepath }, { app: this.appName, appDir: this.appDir });
        expect(runtimeDownloadRequest.isDone()).to.be.false;
        expect(deployRequest.isDone()).to.be.true;
    })
})
