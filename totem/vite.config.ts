import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import tailwindcss from '@tailwindcss/vite';

// https://vite.dev/config/
export default defineConfig({
  plugins: [svelte(), tailwindcss()],
  server: {
    proxy: {
      '/api': {
        target: 'http://esp-home.local:80',
        changeOrigin: true,
        ws: true,
      },
    },
  },
});
