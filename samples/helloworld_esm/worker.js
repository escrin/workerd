const taskScript = () => {
  self.onmessage = (msg) => {
    console.log("inner worker got message:", JSON.stringify(msg.data));
    if (msg.data instanceof CryptoKey) {
      postMessage('got a key');
    }
  };

  addEventListener('tasks', async () => {
    // TODO: nftrout goes here
  });
};

export default {
  async fetch(_req) {
    const script = taskScript.toString().replace(/^\(\) => /, "");
    console.log(script);
    const blob = new Blob([script], { type: 'application/javascript' });
    const scriptUrl = URL.createObjectURL(blob);
    const worker = new Worker(scriptUrl);
    URL.revokeObjectURL(scriptUrl);
    worker.onmessage = (msg) => {
      console.log("got a message from worker:", JSON.stringify(msg));
    };
    worker.onerror = (e) => {
      console.error("worker got an error:", JSON.stringify(e));
    };
    worker.postMessage(await createMinimalCryptoKey());
    worker.postMessage({ message: "hello" });
    return new Response("ok");
  },
}

async function createMinimalCryptoKey() {
  const algorithm = { name: 'AES-GCM', length: 256 }; // Example algorithm (AES-GCM with a key length of 256 bits)
  const extractable = true; // The key can be extracted
  const keyUsages = ['encrypt', 'decrypt']; // Example key usages (encrypt and decrypt)

  try {
    const key = await crypto.subtle.generateKey(algorithm, extractable, keyUsages);
    return key;
  } catch (error) {
    console.error('Error creating CryptoKey:', error);
    return null;
  }
}
