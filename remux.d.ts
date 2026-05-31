declare module '@remux/remux.node' {
	class Remux {
		constructor(options: {
			path: string;
			seek?: number;
			write?: (data: Buffer) => void;
			end?: () => void;
			error?: (e: Error) => void;
		});

		start(): void;
		stop(): void;
		seek(time: number): void;
	}

	function getDuration(path: string): number;
}
