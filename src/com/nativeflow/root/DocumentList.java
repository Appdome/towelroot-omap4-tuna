package com.nativeflow.root;

import java.io.File;
import java.util.Map;
import java.util.List;
import java.util.HashMap;
import java.util.ArrayList;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileNotFoundException;

import android.net.Uri;
import android.util.Log;
import android.view.View;
import android.os.Bundle;
import android.view.Window;
import android.app.Activity;
import android.os.AsyncTask;
import android.os.Environment;
import android.content.Intent;
import android.view.ViewGroup;
import android.content.Context;
import android.widget.TextView;
import android.widget.ListView;
import android.widget.ImageView;
import android.widget.AdapterView;
import android.webkit.MimeTypeMap;
import android.widget.ProgressBar;
import android.widget.ArrayAdapter;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.widget.AdapterView.OnItemClickListener;
import android.content.pm.PackageManager.NameNotFoundException;

public class DocumentList extends Activity
{
	private static String TAG = "DocumentList";

	private ArrayList<File> mAppList = new ArrayList<File>();
	private ListView mListView = null;
	private DocumentListAdapter mAdapter = null;

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
		requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.document_list);

		Intent intent = getIntent();
		String packageName = intent.getStringExtra("packageName");

		mListView = (ListView) super.findViewById(R.id.listview);
		mAdapter = new DocumentListAdapter(this, mAppList);
		mListView.setAdapter(mAdapter);

		mListView.setOnItemClickListener(new OnItemClickListener() {
			public void onItemClick(AdapterView<?> parent, View view,
				int position, long id)
			{
				new CopyFileToSDCardTask(position).execute();
			}
		});

		new FindDocumentsTask().execute(packageName);
	}

	private class CopyFileToSDCardTask extends AsyncTask<Void, Integer, File>
	{
		private int mPosition;

		public CopyFileToSDCardTask(int position)
		{
			super();
			mPosition = position;
		}

		protected File doInBackground(Void... args)
		{
			File sdcardPath = Environment.getExternalStorageDirectory();
			File source = mAdapter.getFile(mPosition);
			File destination = new File(sdcardPath, source.getName());
			long fileSize = source.length();
			int progress = 0;
			Log.i(TAG, "Copying " + source + " to " + destination);
			try {
				InputStream in = new FileInputStream(source);
				OutputStream out = new FileOutputStream(destination);

				byte[] buf = new byte[4 * 1024];
				int read;
				int totalCopied = 0;
				while ((read = in.read(buf)) > 0) {
					out.write(buf, 0, read);
					totalCopied += read;
					if (fileSize != 0) {
						int newProgress = 100 * totalCopied / (int)fileSize;
						if (newProgress > progress) {
							progress = newProgress;
							publishProgress(progress);
						}
					}
				}
				in.close();
				out.close();
			} catch (FileNotFoundException e) {
				Log.e(TAG, "File not found: " + e.getMessage());
				e.printStackTrace();
				return null;
			} catch (IOException e) {
				Log.e(TAG, "I/O error while copying: " + e.getMessage());
				e.printStackTrace();
				return null;
			}
			return destination;
		}

		protected void onProgressUpdate(Integer... progress)
		{
			mAdapter.setCopyProgress(mPosition, progress[0]);
		}

		protected void onPostExecute(File result)
		{
			Intent intent = new Intent(Intent.ACTION_VIEW);
			intent.setData(Uri.fromFile(result));
			/* This seems to confuse viewers, but without it, images can't
			 * be opened.... So seeing as documents are more important, this
			 * is commented out for the time being
			intent.setType(getMimeType(result));
			*/
			startActivity(intent);
			mAdapter.setCopyCompleted(mPosition);
		}
	}

	private static String getMimeType(File file)
	{
		MimeTypeMap mimeTypeMap = MimeTypeMap.getSingleton();
		String url = Uri.fromFile(file).toString();
		if (null == url) {
			return null;
		}
		String extension = mimeTypeMap.getFileExtensionFromUrl(url);
		if (null == extension) {
			return null;
		}
		return mimeTypeMap.getMimeTypeFromExtension(extension);
	}

	private class FindDocumentsTask extends AsyncTask<String, File, Long>
	{
		private final String[] interestingPaths = new String[] {
			"/data/data",
			"/sdcard/Android/data"
		};

		protected void onPreExecute()
		{
			/* TODO: Start throbber */
		}

		private void scanPath(File path)
		{
			for (File file: path.listFiles()) {
				if (file.isFile()) {
					String mimeType = getMimeType(file);
					if (null == mimeType) {
						continue;
					}
					/* We are just interested in documents and the like,
					 * which are under the "application/" class mime-type */
					if (!mimeType.startsWith("application")) {
						continue;
					}
					publishProgress(file);
				} else if (file.isDirectory()) {
					scanPath(file);
				}
			}
		}

		protected Long doInBackground(String... args)
		{
			/* There are two places in which we want to look for application
			 * data:
			 * 1. /data/data/<package-name>
			 * 2. /sdcard/Android/data/<package-name>
			 * For each of them we walk the directory structure and check for
			 * the type of each file we find.
			 * If we stumble over a file that was deemed interesting (TBD) we
			 * add it to the list (and update the UI).
			 */
			String packageName = args[0];
			for (String path: interestingPaths) {
				File dir = new File(path, packageName);
				if (!dir.exists()) {
					continue;
				}
				scanPath(dir);
			}
			return (long)0;
		}

		protected void onProgressUpdate(File... progress)
		{
			mAdapter.add(progress[0]);
			mAdapter.notifyDataSetChanged();
		}

		protected void onPostExecute(Long result)
		{
			/* TODO: Stop throbber */
		}
	}

	private class DocumentListAdapter extends ArrayAdapter<File>
	{
		private Context mContext;
		private ArrayList<File> mValues;
		private Map<Integer, Integer> mProgressMap = new HashMap<Integer, Integer>();

		public DocumentListAdapter(Context context, ArrayList<File> values)
		{
			super(context, R.layout.document_item_layout, R.id.document_name, values);
			this.mContext = context;
			this.mValues = values;
		}

		@Override
		public View getView(int position, View convertView, ViewGroup parent)
		{
			File file = (File)mValues.get(position);
			View rowView = super.getView(position, convertView, parent);

			ImageView imageView = (ImageView)rowView.findViewById(R.id.document_icon);
			try {
				Drawable icon = guessIconForFile(file);
				if (null != icon) {
					imageView.setImageDrawable(icon);
				}
			} catch (NullPointerException e) {
			}

			TextView textView = (TextView)rowView.findViewById(R.id.document_name);
			textView.setText(file.getName());

			ProgressBar progressBar = (ProgressBar)rowView.findViewById(R.id.copy_progress);
			if (mProgressMap.containsKey(position)) {
				progressBar.setVisibility(View.VISIBLE);
				progressBar.setProgress(mProgressMap.get(position));
			} else {
				progressBar.setVisibility(View.GONE);
			}

			return rowView;
		}

		private Drawable guessIconForFile(File file)
		{
			final Intent intent = new Intent(Intent.ACTION_VIEW);
			intent.setData(Uri.fromFile(file));
			intent.setType(getMimeType(file));
			final List<ResolveInfo> matches = mContext.getPackageManager()
				.queryIntentActivities(intent, 0);
			for (ResolveInfo match: matches) {
				return match.loadIcon(mContext.getPackageManager());
			}
			return null;
		}

		public File getFile(int position)
		{
			return (File)mValues.get(position);
		}

		public void setCopyProgress(int position, int progress)
		{
			mProgressMap.put(position, progress);
			notifyDataSetChanged();
		}

		public void setCopyCompleted(int position)
		{
			mProgressMap.remove(position);
			notifyDataSetChanged();
		}
	}
}
