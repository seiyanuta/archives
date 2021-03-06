<template>
  <dashboard-layout title="Code" :appName="appName" no-padding="true">
    <code-editor :code="code" @changed="codeChanged" v-if="editor === 'code'"></code-editor>
    <flow-editor :defs="defs" @changed="codeChanged" :nodes="nodes" v-if="editor === 'flow'"></flow-editor>

    <footer>
      <div class="bottom-bar">
        <template v-if="this.devices.length === 0">
          <div>
            <nuxt-link :to="{ name: 'apps-appName-setup-device', params: { name: this.appName }}">
              <button class="primary">
                <i class="fa fa-magic" aria-hidden="true"></i>
                Setup a device before deploying
              </button>
            </nuxt-link>
            <button @click="deploy">
              <i class="fa fa-deploy" aria-hidden="true"></i>
              Deploy Anyway
            </button>
          </div>
        </template>
        <template v-else>
          <button @click="deploy" class="primary">
            <i class="fa fa-rocket" aria-hidden="true"></i>
            {{ deployButton }}
          </button>
        </template>

        <p class="caption">{{ caption }}</p>
      </div>
      <log-panel :appName="appName"></log-panel>
    </footer>
  </dashboard-layout>
</template>

<script>
import api from "~/assets/js/api"
import { buildApp } from "~/assets/js/build"
import { setLastUsedApp } from "~/assets/js/preferences"
import { defs, buildFlowApp } from "~/assets/js/flow"
import CodeEditor from "~/components/code-editor"
import FlowEditor from "~/components/flow-editor"
import LogPanel from "~/components/log-panel"
import DashboardLayout from "~/components/dashboard-layout"

export default {
  components: { DashboardLayout, CodeEditor, FlowEditor, LogPanel },
  head: {
    link: [
      { rel: 'stylesheet', href: 'https://fonts.googleapis.com/css?family=Source+Code+Pro:400,600' },
    ],
    script: [
      { src: 'https://unpkg.com/babel-standalone@6/babel.min.js' }
    ]
  },
  data() {
    return {
      appName: this.$route.params.appName,
      editor: 'code',
      code: '',
      nodes: {},
      defs,
      devices: [],
      caption: 'Code will be automatically saved.',
      autosaveAfter: 3000,
      deployButton: "Deploy"
    }
  },
  methods: {
    async deploy() {
      let code = this.code
      if (this.editor === 'flow') {
        this.deployButton = "Building Flow..."
        code = await buildFlowApp(this.nodes)
      }

      this.deployButton = "Building..."
      const { image, debug } = await buildApp(code)

      this.deployButton = "Deploying...";
      const comment = "Deployment at " + (new Date()).toString();
      const r = await api.deploy(this.appName, image, debug, comment, null)

      this.deployButton = "Deployed";
      setTimeout(() => {
        this.deployButton = "Deploy";
      }, 3000)
    },
    codeChanged(newCode) {
      this.caption = 'Code will be saved when you stop editing...'
      if (this.autosaveTimer) {
        clearTimeout(this.autosaveTimer)
      }

      this.autosaveTimer = setTimeout(async () => {
        if (this.code === newCode) {
          this.caption = 'Code will be automatically saved.'
        } else {
          this.caption = 'Saving...'
          await api.updateApp(this.appName, { code: newCode })
          this.caption = 'Saved'
          this.code = newCode
        }
      }, this.autosaveAfter)
    }
  },

  async mounted() {
    this.app = await api.getApp(this.appName)
    this.editor = this.app.editor
    this.code = this.app.code

    if (this.editor === 'flow')
      this.nodes = JSON.parse(this.code)

    this.devices = await api.getAppDevices(this.appName)
    setLastUsedApp(this.appName)
  }
};
</script>

<style lang="scss" scoped>
.dashboard-layout {
  background: var(--bg0-color) !important;
  height: 100vh !important;
}

footer {
  .bottom-bar {
    padding: 5px 10px;
    display: flex;
    justify-content: space-between;
    background: var(--bg0-color);

    .caption {
      color: var(--caption-color);
      font-size: 14px;
      display: inline;
    }
  }
}
</style>
