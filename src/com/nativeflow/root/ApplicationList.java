package com.nativeflow.root;

import java.util.ArrayList;

import java.io.File;
import java.io.IOException;

import android.util.Log;
import android.os.Bundle;
import android.view.View;
import android.view.Menu;
import android.view.Window;
import android.os.AsyncTask;
import android.app.Activity;
import android.widget.Button;
import android.content.Intent;
import android.view.ViewGroup;
import android.content.Context;
import android.widget.ListView;
import android.widget.ImageView;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.content.ComponentName;
import android.view.View.OnClickListener;
import android.graphics.drawable.Drawable;
import android.widget.AdapterView.OnItemClickListener;
import android.content.pm.PackageManager.NameNotFoundException;

public class ApplicationList extends Activity
{
	private static String TAG = "ApplicationList";

	private ArrayList<String> mAppList = new ArrayList<String>();
	private ListView mListView = null;
	private ArrayAdapter<String> mAdapter = null;

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
		requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.application_list);

		/* List adapter boilerplate code */
		mListView = (ListView) super.findViewById(R.id.listview);
		mAdapter = new AppListAdapter(this, mAppList);
		mListView.setAdapter(mAdapter);
		mListView.setOnItemClickListener(new OnItemClickListener() {
			public void onItemClick(AdapterView<?> parent, View view,
				int position, long id)
			{
				AppListAdapter adapter = (AppListAdapter)(parent.getAdapter());
				String packageName = adapter.getPackage(position);
				Log.i(TAG, "User clicked " + packageName);

				Intent intent = new Intent();
				intent.setComponent(new ComponentName("com.nativeflow.root",
							"com.nativeflow.root.DocumentList"));
				intent.putExtra("packageName", packageName);
				startActivity(intent);
			}
		});
		populateAppicationList();
    }

	private void populateAppicationList()
	{
		File datadata = new File("/data/data");
		for (File f: datadata.listFiles()) {
			mAdapter.add(f.getName());
		}
		mAdapter.notifyDataSetChanged();
	}

	private class AppListAdapter extends ArrayAdapter<String>
	{
		private Context mContext;
		private ArrayList<String> mValues;

		public AppListAdapter(Context context, ArrayList<String> values)
		{
			super(context, R.layout.rowlayout, R.id.package_name, values);
			this.mContext = context;
			this.mValues = values;
		}

		@Override
		public View getView(int position, View convertView, ViewGroup parent)
		{
			View rowView = super.getView(position, convertView, parent);
			ImageView imageView = (ImageView)rowView.findViewById(R.id.package_icon);
			try {
				Drawable icon = mContext.getPackageManager()
					.getApplicationIcon((String)mValues.get(position));
				imageView.setImageDrawable(icon);
			} catch (NameNotFoundException e) {
			} catch (NullPointerException e) {
			}
			return rowView;
		}

		public String getPackage(int position)
		{
			return (String)mValues.get(position);
		}
	}
}
