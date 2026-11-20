// USV Aqua-Sentinel PWA Service Worker v1.0
const CACHE_NAME = 'aqua-sentinel-v1';
const ASSETS_TO_CACHE = [
  '/',
  '/map',
  '/3d',
  '/cloud',
  '/manifest.json'
];

self.addEventListener('install', (event) => {
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(clients.claim());
});

self.addEventListener('fetch', (event) => {
  // Network first with timeout fallback
  if (event.request.method === 'GET' && !event.request.url.includes('/macros/s/') && !event.request.url.includes('/api/')) {
    event.respondWith(
      fetch(event.request).catch(() => caches.match(event.request))
    );
  }
});
