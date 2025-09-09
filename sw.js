const CACHE = 'scoreboard-pwa-v1';
const ASSETS = [
  './',
  './index.html',
  './styles.css',
  './app.js',
  './ble.js',
  './wifi.js',
  './transport.js',
  './manifest.webmanifest',
];

self.addEventListener('install', (event) => {
  event.waitUntil((async () => {
    const cache = await caches.open(CACHE);
    await cache.addAll(ASSETS);
    self.skipWaiting();
  })());
});

self.addEventListener('activate', (event) => {
  event.waitUntil(self.clients.claim());
});

self.addEventListener('fetch', (event) => {
  const { request } = event;
  // Network-first for API calls to local device; cache-first for app shell
  if (request.url.includes('/api/')) {
    event.respondWith((async () => {
      try {
        const net = await fetch(request);
        return net;
      } catch (e) {
        const cache = await caches.open(CACHE);
        const cached = await cache.match(request);
        return cached || new Response('Offline', { status: 503 });
      }
    })());
    return;
  }
  event.respondWith((async () => {
    const cache = await caches.open(CACHE);
    const cached = await cache.match(request);
    if (cached) return cached;
    try {
      const net = await fetch(request);
      cache.put(request, net.clone());
      return net;
    } catch (e) {
      return cached || new Response('Offline', { status: 503 });
    }
  })());
});
