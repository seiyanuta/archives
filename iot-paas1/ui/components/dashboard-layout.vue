<template>
  <div class="dashboard-layout" :class="{ 'inverted-bg': invertedBg }">
    <header>
      <nav>
        <dropdown :items="navItems" :title-style="navTitleStyle" :selected="navSelected"></dropdown>
      </nav>
      <div class="hamburger">
        <dropdown :items="appSwitcherItems" :selected="appName" :title-style="appSwitcherTitleStyle"></dropdown>
        <div class="avatar-container" :data-balloon="avatarBalloon" data-balloon-pos="down-right">
          <img class="avatar" :src="avatarUrl">
        </div>
      </div>
    </header>

    <main :class="{ 'no-padding': noPadding }">
      <slot></slot>
    </main>
  </div>
</template>

<script>
import Dropdown from "~/components/dropdown"
import api from "~/assets/js/api"
import md5 from "blueimp-md5"

export default {
  components: { Dropdown },
  props: ['title', 'appName', 'no-padding', 'inverted-bg'],
  data() {
    return {
      avatarBalloon: `Logged in as ${api.username}`,
      navTitleStyle: {
        'font-family': '"Roboto", sans-serif',
        'font-weight': 600,
        'font-size': '28px'
      },
      appSwitcherTitleStyle: {
        'font-weight': 600
      },
      navItems: [
        {
          title: 'Code',
          to: { name: 'apps-appName', params: { name: this.appName } }
        },
        {
          title: 'Devices',
          to: { name: 'apps-appName-devices', params: { name: this.appName } }
        },
        {
          title: 'Setup a Device',
          to: { name: 'apps-appName-setup-device', params: { name: this.appName } }
        },
        {
          title: 'Settings',
          to: { name: 'apps-appName-settings', params: { name: this.appName } },
        },
        { divider: true },
        {
          title: 'User Settings',
          to: { name: 'user-settings' }
        },
        { divider: true },
        {
          title: 'Logout',
          to: { name: 'logout' }
        }
      ],
      navSelected: this.title,
      appSwitcherItems: []
    }
  },
  computed: {
    avatarUrl() {
      return "https://www.gravatar.com/avatar/" + md5(api.email) + "?s=30&d=mm";
    }
  },
  async beforeMount() {
    if (!api.loggedIn()) {
      return;
    }

    const appItems = (await api.getApps()).map(app => {
      return {
        title: app.name,
        to: { name: 'apps-appName', params: { appName: app.name } }
      }
    })

    this.appSwitcherItems = [
      ...appItems,
      { divider: true },
      {
        title: 'Create a new app',
        icon: 'plus',
        bold: true,
        to: { name: 'create-app' }
      }
    ]
  },
  beforeCreate() {
    if (!api.loggedIn()) {
      this.$router.push({name: 'login'})
    }
  }
}
</script>

<style>
.desktop header {
  -webkit-app-region: drag;
}
</style>

<style lang="scss" scoped>
.dashboard-layout {
  height: 100%;
  box-sizing: border-box;
  color: var(--fg0-color);
  display: flex;
  flex-direction: column;

  &.inverted-bg {
    background-color: var(--bg1-color);
  }

  & > header {
    display: flex;
    position: sticky;
    top: 0;
    width: 100vw;
    box-sizing: border-box;
    justify-content: space-between;
    align-items: center;
    padding: 20px 8px;
    background-color: var(--bg0-color);
    z-index: 5;

    .hamburger {
      display: flex;
      justify-content: space-between;

      .avatar-container {
        width: fit-content;
        margin-left: 15px;
        cursor: default;
        .avatar {
          border-radius: 15px;
        }
      }
    }
  }

  & > main {
    display: flex;
    flex: 1;
    flex-direction: column;

    padding: 15px 30px;
    &.no-padding {
      padding: 0;
    }
  }
}
</style>
