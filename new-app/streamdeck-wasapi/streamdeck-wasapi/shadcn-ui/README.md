# StreamDeck WASAPI UI

A modern UI for StreamDeck WASAPI Controller built with Next.js and shadcn/ui.

## Features

- Modern, responsive UI with shadcn/ui components
- Drag and drop application management
- Group organization for applications
- Controller status display for connected hardware
- Settings management
- Key shortcut binding system

## Getting Started

### Prerequisites

- Node.js 16.8.0 or later
- npm or yarn

### Installation

1. Clone the repository
2. Navigate to the project directory
3. Install dependencies:

```bash
npm install
# or
yarn install
```

### Running the development server

```bash
npm run dev
# or
yarn dev
```

Open [http://localhost:3000](http://localhost:3000) in your browser to see the application.

### Building for production

```bash
npm run build
# or
yarn build
```

Then, to start the production server:

```bash
npm run start
# or
yarn start
```

### API Configuration

The application connects to a backend API for data operations. The API endpoints are configured in the `API_URLS` object in `src/app/page.tsx`. Make sure these endpoints match your backend implementation.

## Backend Integration

This UI is designed to work with the StreamDeck WASAPI Controller backend. The UI communicates with the backend through HTTP endpoints to:

- Load and save configuration
- Get applications list
- Monitor controller status
- Manage key bindings

## License

[MIT](LICENSE)
