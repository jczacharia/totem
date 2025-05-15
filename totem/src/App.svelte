<script lang="ts">
  import { onMount } from 'svelte';
  import './app.css';
  import { convertImageToLedMatrixBuffer, processGifToFrames } from './lib/image-converter';
  import './vite.svg';

  const MATRIX_WIDTH = 64;
  const MATRIX_HEIGHT = 64;

  let color = $state('#000000');
  let fileInput = $state() as HTMLInputElement;
  let gifInput = $state() as HTMLInputElement;
  let canvasCtx = $state<CanvasRenderingContext2D | null>(null);
  let canvasPreview = $state<HTMLCanvasElement | null>(null);

  onMount(() => {
    if (canvasPreview) {
      canvasCtx = canvasPreview.getContext('2d');

      // Initialize with solid color
      updateSolidColorPreview();
    }
  });

  const rgb = $derived.by(() => {
    let hex = color.replace('#', '');

    if (hex.length === 3) {
      hex = hex[0] + hex[0] + hex[1] + hex[1] + hex[2] + hex[2];
    }

    const red = parseInt(hex.substring(0, 2), 16);
    const green = parseInt(hex.substring(2, 4), 16);
    const blue = parseInt(hex.substring(4, 6), 16);

    return { red, green, blue };
  });

  async function sendRgb() {
    await fetch('/api/v1/matrix/rgb', {
      method: 'POST',
      body: JSON.stringify(rgb),
    });
  }

  function updateSolidColorPreview() {
    if (canvasCtx) {
      canvasCtx.fillStyle = color;
      canvasCtx.fillRect(0, 0, MATRIX_WIDTH, MATRIX_HEIGHT);
    }
  }

  async function handleImageUpload(event: Event) {
    const input = event.target as HTMLInputElement;
    if (input.files && input.files.length > 0) {
      const file = input.files[0];

      try {
        // Convert image to buffer
        const buffer = await convertImageToLedMatrixBuffer(file);
        console.log(buffer.length);

        // Show preview
        displayBufferPreview(buffer);

        // Upload to ESP32
        await fetch('/api/matrix/state/gif', {
          method: 'POST',
          body: buffer.buffer,
        });
      } catch (error) {
        console.error('Error processing image:', error);
      }
    }
  }

  function displayBufferPreview(buffer: Uint32Array) {
    if (!canvasCtx) return;

    const imageData = canvasCtx.createImageData(MATRIX_WIDTH, MATRIX_HEIGHT);

    for (let y = 0; y < MATRIX_HEIGHT; y++) {
      for (let x = 0; x < MATRIX_WIDTH; x++) {
        const pixelIndex = y * MATRIX_WIDTH + x;
        const dataIndex = pixelIndex * 4;

        // Extract RGB from buffer (reverse of the packing process)
        const rgb = buffer[pixelIndex];
        const r = (rgb >> 16) & 0xff;
        const g = (rgb >> 8) & 0xff;
        const b = rgb & 0xff;

        // Set RGBA in the ImageData
        imageData.data[dataIndex] = r;
        imageData.data[dataIndex + 1] = g;
        imageData.data[dataIndex + 2] = b;
        imageData.data[dataIndex + 3] = 255; // Alpha (fully opaque)
      }
    }

    canvasCtx.putImageData(imageData, 0, 0);
  }

  async function handleGifUpload(event: Event) {
    const input = event.target as HTMLInputElement;
    if (input.files && input.files.length > 0) {
      const file = input.files[0];

      if (!file.type.includes('gif')) {
        console.error('Error: Please select a GIF file');
        return;
      }

      try {
        // Process the GIF into frames
        const frames = await processGifToFrames(file, (progress) => {
          console.log(`Processing GIF: ${Math.round(progress * 100)}%`);
        });

        if (frames.length === 0) {
          throw new Error('No frames were extracted from the GIF');
        }
        console.log(frames);
        await fetch('/api/matrix/state/gif', {
          method: 'POST',
          body: frames.buffer,
        });
      } catch (error) {
        console.error('Error processing GIF:', error);
      }
    }
  }
</script>

<main class="grid p-4">
  <div class="flex gap-4">
    <input type="color" bind:value={color} />
    <button onclick={sendRgb}>Send RGB</button>
  </div>

  <div class="section">
    <h3>Image Upload</h3>
    <div class="image-upload">
      <input type="file" bind:this={fileInput} accept="image/*" onchange={handleImageUpload} />
      <button onclick={() => fileInput?.click()}>Select Image</button>
    </div>
  </div>

  <div class="preview-container">
    <canvas
      bind:this={canvasPreview}
      width={MATRIX_WIDTH}
      height={MATRIX_HEIGHT}
      class="matrix-preview"
    ></canvas>
  </div>

  <div class="section">
    <h3>GIF Upload</h3>
    <div class="gif-upload">
      <input type="file" bind:this={gifInput} accept="image/gif" onchange={handleGifUpload} />
      <button onclick={() => gifInput?.click()}>Select GIF</button>
    </div>
  </div>
</main>
